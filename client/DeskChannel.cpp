#include "DeskChannel.h"

#include "Channel.h"
#include "ClientUser.h"
#include "Global.h"
#include "MainWindow.h"
#include "ServerHandler.h"
#include "Version.h"

#include <QtCore/QCryptographicHash>
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

QString DeskChannel::clientVersion() const {
#ifdef RUUMBLE_CLIENT_VERSION
	return QStringLiteral(RUUMBLE_CLIENT_VERSION);
#else
	return QStringLiteral("unknown");
#endif
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

void DeskChannel::showChannelMenu(int channelId, double x, double y) {
	if (m_allowed() && channelId >= 0) {
		emit channelMenuRequested(static_cast< unsigned int >(channelId), QPoint(static_cast< int >(x), static_cast< int >(y)));
	}
}

void DeskChannel::showUserMenu(int session, double x, double y) {
	if (m_allowed() && session > 0) {
		emit userMenuRequested(static_cast< unsigned int >(session), QPoint(static_cast< int >(x), static_cast< int >(y)));
	}
}

void DeskChannel::moveUser(int session, int channelId) {
	if (m_allowed() && session > 0 && channelId >= 0) {
		emit userMoveRequested(static_cast< unsigned int >(session), static_cast< unsigned int >(channelId));
	}
}

namespace {
	QString descriptionHash(const QString &description) {
		return QString::fromLatin1(QCryptographicHash::hash(description.toUtf8(), QCryptographicHash::Sha1).toHex());
	}
} // namespace

QVariantMap DeskChannel::channelProperties(int channelId) {
	const Channel *c = channelId >= 0 ? Channel::get(static_cast< unsigned int >(channelId)) : nullptr;
	if (!m_allowed() || !c) {
		return {};
	}
	// Mumble keeps the hash of every description it has seen (also the long ones it has not fetched yet)
	const QString hash = !c->qbaDescHash.isEmpty() ? QString::fromLatin1(c->qbaDescHash.toHex())
												   : (c->qsDesc.isEmpty() ? QString() : descriptionHash(c->qsDesc));
	return { { QStringLiteral("name"), c->qsName },
			 { QStringLiteral("maxUsers"), static_cast< int >(c->uiMaxUsers) },
			 { QStringLiteral("descriptionHash"), hash } };
}

QVariantMap DeskChannel::updateChannel(int channelId, const QVariantMap &changes) {
	const Channel *c = channelId >= 0 ? Channel::get(static_cast< unsigned int >(channelId)) : nullptr;
	if (!m_allowed() || !c || !Global::get().sh) {
		return { { QStringLiteral("sent"), false } };
	}
	MumbleProto::ChannelState mpcs;
	mpcs.set_channel_id(c->iId);
	QVariantMap result;
	bool any = false;
	if (changes.contains(QStringLiteral("name"))) {
		const QString name = changes.value(QStringLiteral("name")).toString().trimmed();
		if (name.isEmpty()) {
			return { { QStringLiteral("sent"), false } };
		}
		mpcs.set_name(name.toUtf8().toStdString());
		any = true;
	}
	if (changes.contains(QStringLiteral("description"))) {
		// as given: Mumble's own dialog sends the HTML of its rich text editor, Ruumble sends Markdown as plain text
		const QString description = changes.value(QStringLiteral("description")).toString();
		mpcs.set_description(description.toUtf8().toStdString());
		result.insert(QStringLiteral("descriptionHash"), description.isEmpty() ? QString() : descriptionHash(description));
		any = true;
	}
	if (changes.contains(QStringLiteral("position"))) {
		mpcs.set_position(changes.value(QStringLiteral("position")).toInt());
		any = true;
	}
	if (changes.contains(QStringLiteral("maxUsers"))) {
		const int maxUsers = changes.value(QStringLiteral("maxUsers")).toInt();
		if (maxUsers < 0) {
			return { { QStringLiteral("sent"), false } };
		}
		mpcs.set_max_users(static_cast< unsigned int >(maxUsers));
		any = true;
	}
	if (any) {
		Global::get().sh->sendMessage(mpcs);
	}
	result.insert(QStringLiteral("sent"), any);
	return result;
}

QVariantMap DeskChannel::createChannel(int parentId, const QString &name) {
	const Channel *parent = parentId >= 0 ? Channel::get(static_cast< unsigned int >(parentId)) : nullptr;
	const QString trimmed = name.trimmed();
	if (!m_allowed() || !parent || trimmed.isEmpty() || !Global::get().sh) {
		return { { QStringLiteral("sent"), false } };
	}
	// as Mumble's own "Add" dialog does: permanent, no description, position 0, no user limit
	Global::get().sh->createChannel(parent->iId, trimmed, QString(), 0, false, 0);
	return { { QStringLiteral("sent"), true } };
}

QVariantMap DeskChannel::removeChannel(int channelId) {
	const Channel *c = channelId > 0 ? Channel::get(static_cast< unsigned int >(channelId)) : nullptr;
	if (!m_allowed() || !c || !Global::get().sh) {
		return { { QStringLiteral("sent"), false } };
	}
	MumbleProto::ChannelRemove mpcr;
	mpcr.set_channel_id(c->iId);
	Global::get().sh->sendMessage(mpcr);
	return { { QStringLiteral("sent"), true } };
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
