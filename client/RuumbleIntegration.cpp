#include "RuumbleIntegration.h"

#include "DeskChannel.h"
#include "QtMumbleApi.h"
#include "RuumbleView.h"

#include "ClientUser.h"
#include "Global.h"
#include "MainWindow.h"
#include "ServerHandler.h"
#include "UserModel.h"
#include "Version.h"

#include "config.h"
#include "core.h"
#include "messages.h"
#include "net.h"

#include <QtCore/QTimer>
#include <QtGui/QShortcut>
#include <QtWidgets/QDockWidget>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QToolBar>
#include <QtWidgets/QTreeView>

#include <cstdlib>
#include <functional>

#ifndef RUUMBLE_CORE_VERSION
#	define RUUMBLE_CORE_VERSION "0.0.0"
#endif

namespace ruumbledesk {

/**
 * Transport that tells the view which service the core connects to. Ruumble's core stays
 * unchanged: this is the only place that learns the base URL when no pairing link comes.
 */
class ObservingTransport : public ruumble::Transport {
public:
	ObservingTransport(ruumble::Transport &inner, std::function< void(const std::string &) > onConnect)
		: m_inner(inner), m_onConnect(std::move(onConnect)) {}

	void connect(const std::string &baseUrl) override {
		m_onConnect(baseUrl);
		m_inner.connect(baseUrl);
	}
	void disconnect() override { m_inner.disconnect(); }
	void send(const std::string &json) override { m_inner.send(json); }

private:
	ruumble::Transport &m_inner;
	std::function< void(const std::string &) > m_onConnect;
};

namespace {

	/** Settings::TalkState as Mumble_TalkingState (like PluginManager::on_userTalkingStateChanged) */
	int talkingState(Settings::TalkState state) {
		switch (state) {
			case Settings::Passive:
				return 0;
			case Settings::Talking:
				return 1;
			case Settings::Whispering:
				return 2;
			case Settings::Shouting:
				return 3;
			case Settings::MutedTalking:
				return 4;
		}
		return -1;
	}

	/** paired state and fixed address of Ruumble Desk: own file, never the plugin's plugin.json */
	ruumble::Config &config() {
		static ruumble::Config c = [] {
			if (!std::getenv("RUUMBLE_CONFIG")) {
				qputenv("RUUMBLE_CONFIG", Global::get().qdBasePath.filePath(QLatin1String("ruumble.json")).toLocal8Bit());
			}
			return ruumble::Config::load();
		}();
		return c;
	}

} // namespace

RuumbleIntegration::RuumbleIntegration(MainWindow *mw) : QObject(mw), m_mw(mw) {
	// The building is the whole window. Mumble's widgets keep existing (its logic needs them), but stay
	// hidden: the user tree in a dock of its own, log and chat in theirs (applyChrome).
	// Mumble's saved dock arrangement means nothing here (all docks stay hidden), and restoring one saved by an
	// earlier build crashed in Qt's layout: only the window geometry is kept (cleared before MainWindow::loadState)
	Global::get().s.qbaMainWindowState  = QByteArray();
	Global::get().s.qbaMinimalViewState = QByteArray();

	m_desk = new DeskChannel(
		mw, [this] { return m_view && m_view->showsService(); },
		// the page reports CSS pixels: the view's zoom turns them into widget pixels
		[this](const QPoint &css) { showMumbleMenu(css * m_view->zoomFactor()); },
		[this](bool on) { setUserListVisible(on); }, this);
	m_view = new RuumbleView(m_desk, [this](const QString &action) { onAction(action); }, mw);
	m_usersDock = new QDockWidget(tr("Users"), mw);
	m_usersDock->setObjectName(QLatin1String("qdwRuumbleUsers"));
	// takeCentralWidget, not setCentralWidget alone: that would delete the user tree, and Mumble's user model with it
	m_usersDock->setWidget(mw->takeCentralWidget());
	mw->setCentralWidget(m_view);
	mw->addDockWidget(Qt::RightDockWidgetArea, m_usersDock);
	// closed with its own button: stays closed (applyChrome follows m_userListVisible)
	QObject::connect(m_usersDock->toggleViewAction(), &QAction::toggled, this, [this](bool on) { m_userListVisible = on; });
	mw->setContextMenuPolicy(Qt::NoContextMenu); // no menu to bring the docks back
	// F10 opens Mumble's menus, like a menu bar would
	auto *menuKey = new QShortcut(QKeySequence(Qt::Key_F10), mw);
	menuKey->setContext(Qt::ApplicationShortcut);
	QObject::connect(menuKey, &QShortcut::activated, this, [this] { showMumbleMenu(QPoint(m_view->width() - 360, 64)); });
	// Mumble's settings stay one key away, also while no server is connected (the menu bar is gone)
	auto *settingsKey = new QShortcut(QKeySequence::Preferences, mw);
	settingsKey->setContext(Qt::ApplicationShortcut);
	QObject::connect(settingsKey, &QShortcut::activated, mw, [mw] { mw->on_qaConfigDialog_triggered(); });
	if (QKeySequence(QKeySequence::Preferences) != QKeySequence(Qt::CTRL | Qt::Key_Comma)) {
		auto *commaKey = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Comma), mw);
		commaKey->setContext(Qt::ApplicationShortcut);
		QObject::connect(commaKey, &QShortcut::activated, mw, [mw] { mw->on_qaConfigDialog_triggered(); });
	}
	QObject::connect(m_view, &QWebEngineView::titleChanged, this, &RuumbleIntegration::updateWindowTitle);
	QObject::connect(m_view, &QWebEngineView::iconChanged, mw, [mw](const QIcon &icon) {
		if (!icon.isNull()) {
			mw->setWindowIcon(icon);
		}
	});

	QObject::connect(this, &RuumbleIntegration::serviceKnown, m_view, &RuumbleView::showService, Qt::QueuedConnection);
	QObject::connect(this, &RuumbleIntegration::linkRequested, m_view, &RuumbleView::openLink, Qt::QueuedConnection);
	QObject::connect(this, &RuumbleIntegration::placeholderRequested, m_view, &RuumbleView::showPlaceholder,
					 Qt::QueuedConnection);

	m_api       = std::make_unique< QtMumbleApi >();
	m_socket    = std::make_unique< ruumble::WebSocketTransport >();
	m_transport = std::make_unique< ObservingTransport >(
		*m_socket, [this](const std::string &url) { emit serviceKnown(QString::fromStdString(url)); });

	ruumble::Settings settings;
	settings.pluginVersion = RUUMBLE_CORE_VERSION;
	settings.mumbleVersion = Version::toString(Version::get()).toStdString();
	const char *lang       = std::getenv("LANG");
	settings.locale   = ruumble::localeFromEnv(std::getenv("LC_ALL"), std::getenv("LC_MESSAGES"), lang ? lang : "");
	settings.autoOpen = true; // the link opens in the own view, there is no browser to bother
	settings.bridgeUrl = config().bridgeUrl;

	m_core = std::make_unique< ruumble::Core >(
		*m_api, *m_transport, settings,
		[this](const std::string &url) { emit linkRequested(QString::fromStdString(url)); },
		[](const std::string &url) { return config().pairedWith.count(url) > 0; },
		[](const std::string &url) {
			config().pairedWith.insert(url);
			config().save();
		});

	m_socket->onOpen([this] { m_core->onTransportOpen(); });
	m_socket->onClose([this] { m_core->onTransportClosed(); });
	m_socket->onMessage([this](std::string msg) { m_core->onTransportMessage(std::move(msg)); });
	m_socket->onError([this](std::string reason) { m_core->onTransportError(std::move(reason)); });

	// client events, all non-blocking on the core side
	QObject::connect(mw, &MainWindow::serverSynchronized, this, [this] {
		m_core->onSynchronized();
		// no service after a while: the server does not offer Ruumble (or it is not reachable)
		QTimer::singleShot(8000, this, [this] {
			if (Global::get().uiSession != 0 && !m_view->showsService()) {
				m_view->showPlaceholder(true);
			}
		});
	});
	QObject::connect(mw->pmModel, &UserModel::userMoved, this,
					 [this](unsigned int session, std::optional< unsigned int >, unsigned int channel) {
						 m_core->onChannelEntered(session, static_cast< int32_t >(channel));
					 });
	QObject::connect(mw->pmModel, &UserModel::userAdded, this, &RuumbleIntegration::watchTalking);

	m_core->start();
}

RuumbleIntegration::~RuumbleIntegration() {
	stop();
}

void RuumbleIntegration::stop() {
	// Order as in the plugin: first the worker (it drives the network), then the network.
	// A worker waiting for the main thread gives up after QtMumbleApi's timeout, so this cannot hang.
	if (m_core) {
		m_core->stop();
	}
	if (m_socket) {
		m_socket->disconnect();
	}
	m_core.reset();
	m_transport.reset();
	m_socket.reset();
	m_api.reset();
}

void RuumbleIntegration::attachServerHandler(ServerHandler *sh) {
	// Direct connection like the PluginManager: runs in the ServerHandler thread before the
	// disconnect logic; the core only queues the event.
	QObject::connect(
		sh, &ServerHandler::aboutToDisconnect, this,
		[this] {
			if (m_core) {
				m_core->onDisconnected();
			}
			if (m_api) {
				m_api->resetBlobRequest();
			}
			emit placeholderRequested(false);
			QMetaObject::invokeMethod(m_desk, [desk = m_desk] { desk->setServer(QString()); }, Qt::QueuedConnection);
		},
		Qt::DirectConnection);
	QObject::connect(sh, &ServerHandler::connected, this, [this] {
		if (!Global::get().sh) {
			return;
		}
		QString host, user, pw;
		unsigned short port = 0;
		Global::get().sh->getConnectionInfo(host, port, user, pw);
		m_desk->setServer(QStringLiteral("%1:%2").arg(host).arg(port));
	});
}

void RuumbleIntegration::applyChrome() {
	m_mw->menuBar()->setVisible(false);
	m_mw->qtIconToolbar->setVisible(false);
	m_mw->qdwLog->setVisible(false);
	m_mw->qdwChat->setVisible(false);
	m_mw->qdwMinimalViewNote->setVisible(false);
	m_usersDock->setVisible(m_userListVisible);
}

void RuumbleIntegration::setUserListVisible(bool visible) {
	m_userListVisible = visible;
	m_usersDock->setVisible(visible);
}

void RuumbleIntegration::showMumbleMenu(const QPoint &viewPos) {
	// The menu bar's own QMenus, not copies: Mumble fills several of them when they open (aboutToShow), so
	// every entry, state and future addition of Mumble stays as it is.
	if (!m_mumbleMenu) {
		m_mumbleMenu = new QMenu(m_mw);
	}
	m_mumbleMenu->clear();
	for (QAction *action : m_mw->menuBar()->actions()) {
		if (action->menu()) {
			m_mumbleMenu->addMenu(action->menu());
		}
	}
	m_mumbleMenu->addSeparator();
	QAction *users = m_mumbleMenu->addAction(tr("Mumble user list (channels, users, rights)"));
	users->setCheckable(true);
	users->setChecked(m_userListVisible);
	QObject::connect(users, &QAction::toggled, this, &RuumbleIntegration::setUserListVisible);
	QStringList titles;
	for (QAction *action : m_mumbleMenu->actions()) {
		if (action->menu()) {
			titles << action->menu()->title().remove(QLatin1Char('&'));
		}
	}
	qDebug() << "Ruumble Desk: Mumble menu with" << titles;
	m_mumbleMenu->popup(m_view->mapToGlobal(viewPos));
}

void RuumbleIntegration::updateWindowTitle() {
	const QString title = m_view->title();
	// a page without a title shows its URL as title: then the own name
	const bool useful = m_view->showsService() && !title.isEmpty() && !title.startsWith(QLatin1String("http"));
	m_mw->setWindowTitle(useful ? title : QStringLiteral("Ruumble Desk"));
}

void RuumbleIntegration::onAction(const QString &action) {
	if (action == QLatin1String("connect")) {
		m_mw->on_qaServerConnect_triggered();
	} else if (action == QLatin1String("settings")) {
		m_mw->on_qaConfigDialog_triggered();
	} else if (action == QLatin1String("audio")) {
		m_mw->on_qaAudioWizard_triggered();
	}
}

void RuumbleIntegration::watchTalking(unsigned int session) {
	ClientUser *user = ClientUser::get(session);
	if (!user) {
		return;
	}
	// The state is read anew from the session, never through a pointer that may be gone by then.
	QObject::connect(user, &ClientUser::talkingStateChanged, this, [this, session] {
		const ClientUser *u = ClientUser::get(session);
		if (u && m_core) {
			m_core->onTalking(session, talkingState(u->tsState));
		}
	});
}

} // namespace ruumbledesk
