#include "RuumbleIntegration.h"

#include "CityMap.h"
#include "DeskChannel.h"
#include "MumbleMigration.h"
#include "QtMumbleApi.h"
#include "RuumbleView.h"
#include "TrayIcons.h"

#include "Channel.h"
#include "ClientUser.h"
#include "Global.h"
#include "MainWindow.h"
#include "ServerHandler.h"
#include "UserModel.h"
#include "Version.h"

#include "config.h"
#include "core.h"
#include "messages.h"
#ifdef RUUMBLE_CORE_NET_H
#	include RUUMBLE_CORE_NET_H // full path: on Windows "net.h" would find Mumble's src/Net.h
#else
#	include "net.h"
#endif

#include <QtCore/QDateTime>
#include <QtCore/QDir>
#include <QtCore/QDirIterator>
#include <QtCore/QFileInfo>
#include <QtCore/QFile>
#include <QtCore/QStandardPaths>
#include <QtCore/QTimer>
#include <QtCore/QPersistentModelIndex>
#include <QtWebEngineCore/QWebEnginePage>
#include <QtWidgets/QMessageBox>
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

	/** paired state and fixed address of Ruumble Client: own file, never the plugin's plugin.json */
	ruumble::Config &config() {
		static ruumble::Config c = [] {
			if (!std::getenv("RUUMBLE_CONFIG")) {
				qputenv("RUUMBLE_CONFIG", Global::get().qdBasePath.filePath(QLatin1String("ruumble.json")).toLocal8Bit());
			}
			return ruumble::Config::load();
		}();
		return c;
	}

	/**
	 * The pairing cookie in the view belongs to one certificate. A pairing is therefore remembered per service AND
	 * certificate ("<url> <hash>", a URL never contains a space): after the certificate changed (an import of Mumble's
	 * settings, a new certificate) the service sends a new pairing link instead of leaving the view on the old
	 * identity, where it says "Mumble is not connected" while the client is connected. Entries with the URL alone
	 * (up to 0.41.0) do not say which certificate they were for and count as not paired: one new link each.
	 */
	std::string pairingKey(const std::string &url, const std::string &certHash) {
		return url + " " + certHash;
	}

} // namespace

RuumbleIntegration::RuumbleIntegration(MainWindow *mw) : QObject(mw), m_mw(mw) {
	// The building is the whole window. Mumble's widgets keep existing (its logic needs them), but stay
	// hidden: the user tree in a dock of its own, log and chat in theirs (applyChrome).
	// Mumble's saved dock arrangement means nothing here (all docks stay hidden), and restoring one saved by an
	// earlier build crashed in Qt's layout: only the window geometry is kept (cleared before MainWindow::loadState)
	Global::get().s.qbaMainWindowState  = QByteArray();
	Global::get().s.qbaMinimalViewState = QByteArray();
	// Ruumble's own icons for the window and every tray state, before main() creates the tray icon
	applyTrayIcons(mw);
	quietBalloons();

	m_desk = new DeskChannel(
		mw, [this] { return m_view && m_view->showsService(); },
		// the page reports CSS pixels: the view's zoom turns them into widget pixels
		[this](const QPoint &css) { showMumbleMenu(css * m_view->zoomFactor()); },
		[this](bool on) { setUserListVisible(on); }, this);
	m_cityMap = new CityMap(
		mw, [this] { return m_view && m_view->showsCityMap(); }, [this] { m_view->showCityMap(); },
		[this] { m_view->leaveCityMap(Global::get().uiSession != 0); }, this);
	m_view = new RuumbleView(m_desk, m_cityMap, [this](const QString &action) { onAction(action); }, mw);
	QObject::connect(m_view, &QWebEngineView::urlChanged, this, [this](const QUrl &url) {
		if (!RuumbleView::isCityMap(url)) {
			m_cityMap->pageLeft();
		}
	});
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
	// right click and drag and drop in the building (queued from the page's call, see DeskChannel)
	QObject::connect(m_desk, &DeskChannel::channelMenuRequested, this, &RuumbleIntegration::showChannelMenu,
					 Qt::QueuedConnection);
	QObject::connect(m_desk, &DeskChannel::userMenuRequested, this, &RuumbleIntegration::showUserMenu,
					 Qt::QueuedConnection);
	QObject::connect(m_desk, &DeskChannel::userMoveRequested, this, &RuumbleIntegration::moveUser,
					 Qt::QueuedConnection);
	// an older service's web UI has no Desk panel and so no log or chat: then Mumble's own docks are shown
	QObject::connect(m_desk, &DeskChannel::panelSeen, this, [this] {
		m_panelSeen = true;
		if (!m_logChosen && m_logVisible) {
			setLogVisible(false);
		}
	});
	QObject::connect(m_view, &QWebEngineView::loadStarted, this, [this] {
		m_panelSeen = false;
		m_desk->releasePage(); // a press or latch from the page that goes away must not keep transmitting (#9)
	});
	QObject::connect(m_view->page(), &QWebEnginePage::renderProcessTerminated, m_desk, &DeskChannel::releasePage);
	QObject::connect(m_view, &QWebEngineView::loadFinished, this, [this](bool ok) {
		if (!ok || !m_view->showsService()) {
			return;
		}
		QTimer::singleShot(4000, this, [this] {
			if (!m_logChosen && !m_panelSeen && m_view->showsService()) {
				setLogVisible(true);
			}
		});
	});
	QObject::connect(m_view, &QWebEngineView::iconChanged, mw, [mw](const QIcon &icon) {
		if (!icon.isNull()) {
			mw->setWindowIcon(icon);
		}
	});

	QObject::connect(this, &RuumbleIntegration::serviceKnown, m_view, &RuumbleView::showService, Qt::QueuedConnection);
	QObject::connect(this, &RuumbleIntegration::linkRequested, m_view, &RuumbleView::openLink, Qt::QueuedConnection);
	QObject::connect(
		this, &RuumbleIntegration::placeholderRequested, this,
		[this](bool connected) {
			// the City Map stays on screen (a building is being entered, or chosen): only forget the old service
			if (m_cityMap->onScreen()) {
				m_view->forgetService();
			} else {
				m_view->showPlaceholder(connected);
			}
		},
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
	settings.hoverHint = false; // QtMumbleApi requests the root description itself, nobody has to hover

	// the certificate of the current connection, as the core sends it in its hello
	auto ownCertHash = [api = m_api.get()]() -> std::string {
		const std::optional< uint32_t > session = api->localSession();
		return session ? api->userHash(*session) : std::string();
	};
	m_core = std::make_unique< ruumble::Core >(
		*m_api, *m_transport, settings,
		[this](const std::string &url) { emit linkRequested(QString::fromStdString(url)); },
		// both run on the core's worker thread, like the core's own MumbleApi calls
		[ownCertHash](const std::string &url) {
			const std::string hash = ownCertHash();
			return !hash.empty() && config().pairedWith.count(pairingKey(url, hash)) > 0;
		},
		[ownCertHash](const std::string &url) {
			const std::string hash = ownCertHash();
			if (hash.empty()) {
				return;
			}
			auto &paired = config().pairedWith;
			paired.erase(url); // the entry without a certificate (up to 0.41.0)
			paired.insert(pairingKey(url, hash));
			config().save();
		});

	m_socket->onOpen([this] { m_core->onTransportOpen(); });
	m_socket->onClose([this] { m_core->onTransportClosed(); });
	m_socket->onMessage([this](std::string msg) { m_core->onTransportMessage(std::move(msg)); });
	m_socket->onError([this](std::string reason) { m_core->onTransportError(std::move(reason)); });

	// client events, all non-blocking on the core side
	QObject::connect(mw, &MainWindow::serverSynchronized, this, [this] {
		m_core->onSynchronized();
		m_cityMap->entered();
		// no service after a while: the server does not offer Ruumble (or it is not reachable)
		QTimer::singleShot(8000, this, [this] {
			if (Global::get().uiSession != 0 && !m_view->showsService() && !m_view->showsCityMap()) {
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

bool RuumbleIntegration::openCityMap(bool autoconnect) {
	return m_cityMap->open(autoconnect);
}

bool RuumbleIntegration::connectionFailed(int socketError, const QString &reason, int rejectType) {
	return m_cityMap->connectionFailed(socketError, reason, rejectType);
}

void RuumbleIntegration::applyChrome() {
	m_mw->menuBar()->setVisible(false);
	m_mw->qtIconToolbar->setVisible(false);
	m_mw->qdwLog->setVisible(m_logVisible);
	m_mw->qdwChat->setVisible(m_logVisible);
	m_mw->qdwMinimalViewNote->setVisible(false);
	m_usersDock->setVisible(m_userListVisible);
}

void RuumbleIntegration::setUserListVisible(bool visible) {
	m_userListVisible = visible;
	m_usersDock->setVisible(visible);
}

void RuumbleIntegration::setLogVisible(bool visible) {
	m_logVisible = visible;
	qInfo("Ruumble Client: Mumble log and chat %s", visible ? "shown" : "hidden");
	m_mw->qdwLog->setVisible(visible);
	m_mw->qdwChat->setVisible(visible);
}

namespace {
	/** Mumble picks a menu's target from the selection in its user tree first (MainWindow::getContextMenuTargets),
	 * and that tree sits in the hidden users dock here: select the target for the menu, restore the old selection
	 * afterwards (it is also the target of Mumble's chat bar) */
	class TreeSelection {
	public:
		TreeSelection(MainWindow *mw, const QModelIndex &target) : m_mw(mw), m_old(mw->qtvUsers->currentIndex()) {
			m_mw->qpContextPosition = QPoint();
			m_mw->qtvUsers->setCurrentIndex(target);
		}
		~TreeSelection() {
			m_mw->cContextChannel.clear();
			m_mw->cuContextUser.clear();
			if (m_old.isValid()) {
				m_mw->qtvUsers->setCurrentIndex(m_old);
			}
		}

	private:
		MainWindow *m_mw;
		QPersistentModelIndex m_old;
	};

	bool connectedToServer() { return Global::get().sh && Global::get().uiSession != 0 && ClientUser::get(Global::get().uiSession); }

	QStringList actionTitles(const QMenu *menu) {
		QStringList titles;
		for (const QAction *action : menu->actions()) {
			if (!action->isSeparator() && action->isVisible()) {
				titles << action->text().remove(QLatin1Char('&'));
			}
		}
		return titles;
	}
} // namespace

void RuumbleIntegration::showChannelMenu(unsigned int channelId, const QPoint &viewPos) {
	Channel *channel = Channel::get(channelId);
	if (!connectedToServer() || !channel) {
		return;
	}
	const QPoint global = m_view->mapToGlobal(viewPos * m_view->zoomFactor());
	TreeSelection selection(m_mw, m_mw->pmModel->index(channel));
	m_mw->cContextChannel = channel;

	// after Mumble has filled the menu (its aboutToShow runs first): moving a room to another floor, which Mumble
	// only offers as a drag in its tree
	const QMetaObject::Connection extra = QObject::connect(m_mw->qmChannel, &QMenu::aboutToShow, this, [this, channelId] {
		Channel *c = Channel::get(channelId);
		Channel *root = Channel::get(Mumble::ROOT_CHANNEL_ID);
		if (!c || !root || c == root) {
			return;
		}
		// a floor with rooms below another floor: its rooms would be a third level, which the building cannot show
		if (c->cParent == root && !c->qlChannels.isEmpty()) {
			return;
		}
		QMenu *floors = new QMenu(tr("Move to floor"), m_mw->qmChannel);
		QObject::connect(m_mw->qmChannel, &QMenu::aboutToHide, floors, &QObject::deleteLater);
		for (Channel *floor : root->qlChannels) {
			if (floor == c || floor == c->cParent) {
				continue;
			}
			QAction *to = floors->addAction(floor->qsName);
			const unsigned int parentId = floor->iId;
			QObject::connect(to, &QAction::triggered, this, [channelId, parentId] {
				if (!Global::get().sh) {
					return;
				}
				MumbleProto::ChannelState mpcs;
				mpcs.set_channel_id(channelId);
				mpcs.set_parent(parentId);
				Global::get().sh->sendMessage(mpcs);
			});
		}
		if (c->cParent != root) {
			floors->addSeparator();
			QAction *own = floors->addAction(tr("As a floor of its own"));
			QObject::connect(own, &QAction::triggered, this, [channelId] {
				if (!Global::get().sh) {
					return;
				}
				MumbleProto::ChannelState mpcs;
				mpcs.set_channel_id(channelId);
				mpcs.set_parent(Mumble::ROOT_CHANNEL_ID);
				Global::get().sh->sendMessage(mpcs);
			});
		}
		if (!floors->isEmpty()) {
			m_mw->qmChannel->addSeparator();
			m_mw->qmChannel->addMenu(floors);
		}
		qInfo() << "Ruumble Client: channel menu for" << c->qsName << actionTitles(m_mw->qmChannel);
	});
	// the building has two levels (ADR-0007): floors (below the root) and rooms (below a floor). "Add" on a room would
	// make a third level, which locks the whole floor, so a room's menu does not offer it. Mumble's action is shared
	// with its own tree, hence shown again right after
	const bool room = channel->cParent && channel->cParent->cParent;
	m_mw->qaChannelAdd->setVisible(!room);
	m_mw->qmChannel->exec(global);
	m_mw->qaChannelAdd->setVisible(true);
	QObject::disconnect(extra);
}

void RuumbleIntegration::showUserMenu(unsigned int session, const QPoint &viewPos) {
	ClientUser *user = ClientUser::get(session);
	if (!connectedToServer() || !user) {
		return;
	}
	const QPoint global = m_view->mapToGlobal(viewPos * m_view->zoomFactor());
	TreeSelection selection(m_mw, m_mw->pmModel->index(user));
	m_mw->cuContextUser = user;
	const QMetaObject::Connection log = QObject::connect(m_mw->qmUser, &QMenu::aboutToShow, this, [this, user] {
		qInfo() << "Ruumble Client: user menu for" << user->qsName << actionTitles(m_mw->qmUser);
	});
	m_mw->qmUser->exec(global);
	QObject::disconnect(log);
}

void RuumbleIntegration::moveUser(unsigned int session, unsigned int channelId) {
	ClientUser *user = ClientUser::get(session);
	Channel *channel = Channel::get(channelId);
	if (!connectedToServer() || !user || !channel || user->cChannel == channel) {
		return;
	}
	// another person: as Mumble's tree does it, after the "user dragging" setting
	if (session != Global::get().uiSession) {
		switch (Global::get().s.ceUserDrag) {
			case Settings::Ask:
				if (QMessageBox::question(m_mw, QLatin1String("Ruumble Client"),
										  tr("Move %1 to %2?").arg(user->qsName, channel->qsName), QMessageBox::Yes,
										  QMessageBox::No)
					!= QMessageBox::Yes) {
					return;
				}
				break;
			case Settings::DoNothing:
				Global::get().l->log(Log::Information,
									 tr("You have User Dragging set to \"Do Nothing\" so the user wasn't moved."));
				return;
			case Settings::Move:
				break;
		}
	}
	qInfo() << "Ruumble Client: moving" << user->qsName << "to" << channel->qsName;
	MumbleProto::UserState mpus;
	mpus.set_session(session);
	mpus.set_channel_id(channelId);
	Global::get().sh->sendMessage(mpus);
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
	// Mumble's own connection dialog stays reachable (the City Map takes its place everywhere else)
	QAction *servers = m_mumbleMenu->addAction(tr("Mumble server list"));
	QObject::connect(servers, &QAction::triggered, this, [this] {
		m_cityMap->bypassOnce();
		m_mw->on_qaServerConnect_triggered();
	});
	QAction *users = m_mumbleMenu->addAction(tr("Mumble user list (channels, users, rights)"));
	users->setCheckable(true);
	users->setChecked(m_userListVisible);
	QObject::connect(users, &QAction::toggled, this, &RuumbleIntegration::setUserListVisible);
	QAction *log = m_mumbleMenu->addAction(tr("Mumble log and chat"));
	log->setCheckable(true);
	log->setChecked(m_logVisible);
	QObject::connect(log, &QAction::toggled, this, [this](bool on) {
		m_logChosen = true;
		setLogVisible(on);
	});
	QStringList titles;
	for (QAction *action : m_mumbleMenu->actions()) {
		if (action->menu()) {
			titles << action->menu()->title().remove(QLatin1Char('&'));
		}
	}
	qDebug() << "Ruumble Client: Mumble menu with" << titles;
	m_mumbleMenu->popup(m_view->mapToGlobal(viewPos));
}

void RuumbleIntegration::updateWindowTitle() {
	const QString title = m_view->title();
	// a page without a title shows its URL as title: then the own name
	const bool useful = m_view->showsService() && !title.isEmpty() && !title.startsWith(QLatin1String("http"));
	m_mw->setWindowTitle(useful ? title : QStringLiteral("Ruumble Client"));
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

namespace {
	/** copies every file of from into to that to does not have yet; mumble.lock and the console logs stay behind */
	void copyMissing(const QString &from, const QString &to) {
		QDirIterator it(from, QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
		while (it.hasNext()) {
			const QString source = it.next();
			const QString name   = it.fileName();
			if (name == QLatin1String("mumble.lock") || name.startsWith(QLatin1String("Console"))) {
				continue;
			}
			const QString target = QDir(to).filePath(QDir(from).relativeFilePath(source));
			if (!QFile::exists(target)) {
				QDir().mkpath(QFileInfo(target).absolutePath());
				QFile::copy(source, target);
			}
		}
	}
} // namespace

void RuumbleIntegration::takeOverDeskSettings() {
	const QString config = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
	const QString marker = QDir(config).filePath(QStringLiteral("ruumble-desk-taken-over"));
	const QString oldConfig = QDir(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation))
								  .filePath(QStringLiteral("Ruumble/Ruumble Desk"));
	if (QFile::exists(marker) || !QDir(oldConfig).exists()) {
		return;
	}
	const QString data    = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
	const QString oldData = QDir(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation))
								.filePath(QStringLiteral("Ruumble/Ruumble Desk"));
	copyMissing(oldConfig, config);
	if (QDir(oldData).exists()) {
		copyMissing(oldData, data);
	}
	QDir().mkpath(config);
	QFile file(marker);
	if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		file.write(QStringLiteral("%1 from %2 and %3\n")
					   .arg(QDateTime::currentDateTime().toString(Qt::ISODate), oldConfig, oldData)
					   .toUtf8());
	}
	qInfo("Ruumble Client: took over the settings of Ruumble Desk from %s", qPrintable(oldConfig));
}

void RuumbleIntegration::importMumbleSettings() {
	takeOverDeskSettings(); // first, so a former Ruumble Desk keeps its own settings
	MumbleMigration::run();
}

} // namespace ruumbledesk
