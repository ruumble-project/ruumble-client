#include "DeskChannel.h"

#include "Channel.h"
#include "ClientUser.h"
#include "Global.h"
#include "MainWindow.h"
#include "ServerHandler.h"
#include "Version.h"

#include <QtCore/QDateTime>
#include <QtCore/QTimer>

namespace ruumbledesk {

namespace {

	/** what the log keeps for the page: enough for a working day, not the whole history */
	constexpr std::size_t kHistory = 500;

	QString kindOf(Log::MsgType type, bool own) {
		if (own) {
			return QStringLiteral("own");
		}
		switch (type) {
			case Log::TextMessage:
				return QStringLiteral("text");
			case Log::PrivateTextMessage:
				return QStringLiteral("private");
			case Log::Warning:
			case Log::CriticalError:
			case Log::PermissionDenied:
			case Log::YouKicked:
				return QStringLiteral("warning");
			default:
				return QStringLiteral("info");
		}
	}

} // namespace

DeskChannel::DeskChannel(MainWindow *mw, std::function< bool() > allowed,
						 std::function< void(const QPoint &) > showMenu, std::function< void(bool) > showUserList,
						 QObject *parent)
	: QObject(parent), m_mw(mw), m_allowed(std::move(allowed)), m_showMenu(std::move(showMenu)),
	  m_showUserList(std::move(showUserList)) {
	attachLog();
}

void DeskChannel::attachLog() {
	// Mumble creates the Log only after the main window: wait for it
	if (!Global::get().l) {
		QTimer::singleShot(0, this, &DeskChannel::attachLog);
		return;
	}
	QObject::connect(Global::get().l, &Log::messageLogged, this, &DeskChannel::onLogged);
}

QString DeskChannel::version() const {
	return Version::toString(Version::get());
}

void DeskChannel::onLogged(Log::MsgType type, const QString &plain, bool own) {
	if (plain.trimmed().isEmpty()) {
		return;
	}
	QVariantMap entry;
	entry.insert(QStringLiteral("id"), m_nextId++);
	entry.insert(QStringLiteral("time"), QDateTime::currentMSecsSinceEpoch());
	entry.insert(QStringLiteral("kind"), kindOf(type, own));
	entry.insert(QStringLiteral("text"), plain);
	m_history.push_back(entry);
	while (m_history.size() > kHistory) {
		m_history.pop_front();
	}
	emit logged(entry);
}

QVariantList DeskChannel::history() {
	QVariantList list;
	if (!m_allowed()) {
		return list;
	}
	emit panelSeen();
	for (const QVariantMap &entry : m_history) {
		list.append(entry);
	}
	return list;
}

bool DeskChannel::sendMessage(const QString &text) {
	if (!m_allowed() || !Global::get().sh || Global::get().uiSession == 0) {
		return false;
	}
	const QString trimmed = text.trimmed();
	const ClientUser *self = ClientUser::get(Global::get().uiSession);
	if (trimmed.isEmpty() || !self || !self->cChannel) {
		return false;
	}
	// to the own channel, as Mumble's chat bar does by default; plain text becomes escaped HTML
	const QString html = trimmed.toHtmlEscaped().replace(QLatin1Char('\n'), QLatin1String("<br />"));
	Global::get().sh->sendChannelTextMessage(self->cChannel->iId, html, false);
	Global::get().l->log(Log::TextMessage, MainWindow::tr("To %1: %2").arg(Log::formatChannel(self->cChannel), html),
						 MainWindow::tr("Message to channel %1").arg(self->cChannel->qsName), true);
	return true;
}

// The dialogs run their own event loop: started later, so the call from the page returns first.
void DeskChannel::openSettings() {
	if (m_allowed()) {
		QTimer::singleShot(0, m_mw, [mw = m_mw] { mw->on_qaConfigDialog_triggered(); });
	}
}

void DeskChannel::openConnect() {
	if (m_allowed()) {
		QTimer::singleShot(0, m_mw, [mw = m_mw] { mw->on_qaServerConnect_triggered(); });
	}
}

void DeskChannel::openAudioWizard() {
	if (m_allowed()) {
		QTimer::singleShot(0, m_mw, [mw = m_mw] { mw->on_qaAudioWizard_triggered(); });
	}
}

void DeskChannel::disconnectServer() {
	if (m_allowed()) {
		QTimer::singleShot(0, m_mw, [mw = m_mw] { mw->on_qaServerDisconnect_triggered(); });
	}
}

void DeskChannel::showMenu(double x, double y) {
	if (m_allowed()) {
		m_showMenu(QPoint(static_cast< int >(x), static_cast< int >(y))); // CSS pixels of the page
	}
}

void DeskChannel::showUserList(bool on) {
	if (m_allowed()) {
		m_showUserList(on);
	}
}

void DeskChannel::setServer(const QString &server) {
	if (server != m_server) {
		m_server = server;
		emit serverChanged();
	}
}

} // namespace ruumbledesk
