#include "DeskChannel.h"

#include "TransmitControl.h"

#include "ACL.h"
#include "Channel.h"
#include "ChannelListenerManager.h"
#include "ClientUser.h"
#include "Global.h"
#include "MainWindow.h"
#include "ServerHandler.h"
#include "Version.h"
#include "VolumeAdjustment.h"

#include <QtCore/QCryptographicHash>
#include <QtCore/QDateTime>
#include <QtCore/QSettings>
#include <QtGui/QTextDocumentFragment>
#include <QtCore/QTimer>

#include <algorithm>
#include <cmath>

namespace ruumbledesk {

namespace {

	/** what the log keeps for the page: enough for a working day, not the whole history */
	constexpr std::size_t kHistory = 500;

	/** the volume slider of the page (dB), as in server#19 */
	constexpr int kListenMinDb = -30;
	constexpr int kListenMaxDb = 12;
	/** how long a request to listen waits for the server; a refusal after that is not taken for it */
	constexpr int kListenAskMs = 5000;
	/** Ruumble Client's listener attenuation when switched on: strong, the own room comes first */
	constexpr float kQuieterFactor = 0.25f;
	/** marks that Ruumble Client switched the attenuation on once: after that the user's choice stays */
	const QString kQuieterDefaulted = QStringLiteral("listen/quieterDefaulted");

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
	m_transmit = new TransmitControl(mw, this);
	QObject::connect(m_transmit, &TransmitControl::modeChanged, this, &DeskChannel::transmitModeChanged);
	QObject::connect(m_transmit, &TransmitControl::transmittingChanged, this, &DeskChannel::transmittingChanged);
	QObject::connect(m_transmit, &TransmitControl::levelChanged, this, &DeskChannel::micLevelChanged);
	QObject::connect(m_transmit, &TransmitControl::latchedChanged, this, &DeskChannel::latchedChanged);
	QObject::connect(m_transmit, &TransmitControl::keyChanged, this, &DeskChannel::pttKeyChanged);
	QObject::connect(m_transmit, &TransmitControl::optionsChanged, this, &DeskChannel::pttOptionsChanged);
	QObject::connect(m_transmit, &TransmitControl::keyCaptured, this, [this](const QVariantMap &key) {
		if (m_allowed()) {
			emit keyCaptured(key);
		}
	});
	m_volumeTimer = new QTimer(this);
	m_volumeTimer->setSingleShot(true);
	m_volumeTimer->setInterval(250);
	QObject::connect(m_volumeTimer, &QTimer::timeout, this, &DeskChannel::sendListeningVolumes);
	attachLog();
	attachListening();
}

void DeskChannel::attachListening() {
	// the listener manager exists from Mumble's start; the main window's signals report the server's confirmations
	if (!Global::get().channelListenerManager) {
		QTimer::singleShot(0, this, &DeskChannel::attachListening);
		return;
	}
	const auto own = [this](ClientUser *user, Channel *channel) {
		if (!user || user->uiSession != Global::get().uiSession) {
			return;
		}
		if (channel) {
			m_listenAsked.remove(channel->iId);
		}
		emit listeningChanged();
	};
	QObject::connect(m_mw, &MainWindow::userAddedChannelListener, this, own);
	QObject::connect(m_mw, &MainWindow::userRemovedChannelListener, this, own);
	QObject::connect(Global::get().channelListenerManager.get(), &ChannelListenerManager::localVolumeAdjustmentsChanged, this,
					 [this](unsigned int, float, float) { emit listeningChanged(); });
}

void DeskChannel::defaultQuieter() {
	// "quieter when someone talks in my room": on by default, once, when a page first listens in (a web UI that can
	// show the switch); the user's choice stays after that
	QSettings settings;
	if (settings.value(kQuieterDefaulted, false).toBool()) {
		return;
	}
	settings.setValue(kQuieterDefaulted, true);
	if (!listenQuieter()) {
		setQuieter(true);
	}
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
	// Mumble reports a refused listening only as a "permission denied" line in its log, with the texts of
	// MainWindow::msgPermissionDenied: the missing Listen permission names the channel, the two limits name none (they
	// belong to the last ask). Any other refusal (entering a closed room, ...) is not taken for an ask
	if (type == Log::PermissionDenied && !m_listenAsked.isEmpty()) {
		const auto text = [](const QString &html) { return QTextDocumentFragment::fromHtml(html).toPlainText(); };
		QString reason;
		unsigned int refused = m_lastAskedChannel;
		if (plain == text(MainWindow::tr("No more listeners allowed in this channel."))) {
			reason = QStringLiteral("channel-limit");
		} else if (plain == text(MainWindow::tr("You are not allowed to listen to more channels than you currently are."))) {
			reason = QStringLiteral("user-limit");
		} else {
			for (auto it = m_listenAsked.cbegin(); it != m_listenAsked.cend(); ++it) {
				const Channel *c = Channel::get(it.key());
				if (c
					&& plain
						   == text(MainWindow::tr("You were denied %1 privileges in %2.")
									   .arg(ChanACL::permName(ChanACL::Listen), c->qsName.toHtmlEscaped()))) {
					reason  = QStringLiteral("permission");
					refused = it.key();
					break;
				}
			}
		}
		if (!reason.isEmpty() && m_listenAsked.remove(refused) && m_allowed()) {
			emit listeningRefused(static_cast< int >(refused), reason);
		}
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

// How the user transmits (ticket #9): TransmitControl does the work, the page only asks and presses.
QString DeskChannel::transmitMode() const {
	return m_transmit->mode();
}

bool DeskChannel::transmitting() const {
	return m_transmit->transmitting();
}

double DeskChannel::micLevel() const {
	return m_transmit->level();
}

bool DeskChannel::latched() const {
	return m_transmit->latched();
}

QVariantMap DeskChannel::pttKey() const {
	return m_transmit->key();
}

QVariantMap DeskChannel::pttOptions() const {
	return m_transmit->options();
}

bool DeskChannel::setTransmitMode(const QString &mode) {
	return m_allowed() && m_transmit->setMode(mode);
}

void DeskChannel::pushToTalk(bool down) {
	// letting go is always allowed: a press must never stay down because the page changed in between
	if (!down || m_allowed()) {
		m_transmit->press(down);
	}
}

void DeskChannel::setLatched(bool on) {
	if (!on || m_allowed()) {
		m_transmit->setLatched(on);
	}
}

void DeskChannel::watchLevel(bool on) {
	m_transmit->watchLevel(on && m_allowed());
}

bool DeskChannel::captureKey() {
	return m_allowed() && m_transmit->startCapture();
}

void DeskChannel::capturePageKey(const QStringList &codes) {
	if (m_allowed()) {
		m_transmit->capturePageKey(codes);
	}
}

void DeskChannel::cancelCapture() {
	m_transmit->cancelCapture();
}

bool DeskChannel::acceptKey() {
	return m_allowed() && m_transmit->acceptKey();
}

bool DeskChannel::setPttOption(const QString &name, const QVariant &value) {
	return m_allowed() && m_transmit->setOption(name, value);
}

void DeskChannel::releasePage() {
	m_transmit->releasePage();
	m_transmit->cancelCapture();
	m_transmit->watchLevel(false);
}

void DeskChannel::setServer(const QString &server) {
	if (server != m_server) {
		m_server = server;
		emit serverChanged();
		// Mumble forgets the listening on disconnect, the server sends it again on connect
		m_listenAsked.clear();
		emit listeningChanged();
	}
}

QVariantList DeskChannel::listening() const {
	QVariantList result;
	const auto &manager = Global::get().channelListenerManager;
	const unsigned int self = Global::get().uiSession;
	if (!manager || self == 0) {
		return result;
	}
	for (unsigned int channel : manager->getListenedChannelsForUser(self)) {
		const float factor = manager->getListenerVolumeAdjustment(self, channel).factor;
		const double db = factor > 0 ? std::round(VolumeAdjustment::toDBAdjustment(factor)) : kListenMinDb;
		result.append(QVariantMap{ { QStringLiteral("channel"), static_cast< int >(channel) }, { QStringLiteral("volume"), db } });
	}
	return result;
}

bool DeskChannel::listenQuieter() const {
	return Global::get().s.listenerAttenuationFactor < 1.0f || Global::get().s.alwaysAttenuateListeners;
}

void DeskChannel::startListening(int channelId) {
	const Channel *c = channelId >= 0 ? Channel::get(static_cast< unsigned int >(channelId)) : nullptr;
	if (!m_allowed() || !c || !Global::get().sh || Global::get().uiSession == 0) {
		return;
	}
	const unsigned int id = c->iId;
	const quint64 ask     = ++m_lastAsk;
	m_listenAsked.insert(id, ask);
	m_lastAskedChannel = id;
	QTimer::singleShot(kListenAskMs, this, [this, id, ask] {
		if (m_listenAsked.value(id) == ask) {
			m_listenAsked.remove(id);
		}
	});
	defaultQuieter();
	Global::get().sh->startListeningToChannel(id);
}

void DeskChannel::stopListening(const QVariantList &channelIds) {
	if (!m_allowed() || !Global::get().sh) {
		return;
	}
	QList< unsigned int > ids;
	for (const QVariant &v : channelIds) {
		bool ok        = false;
		const int id   = v.toInt(&ok);
		if (ok && id >= 0) {
			ids.append(static_cast< unsigned int >(id));
		}
	}
	if (!ids.isEmpty()) {
		Global::get().sh->stopListeningToChannels(ids);
	}
}

void DeskChannel::setListeningVolume(int channelId, double db) {
	const auto &manager = Global::get().channelListenerManager;
	const unsigned int self = Global::get().uiSession;
	if (!m_allowed() || !manager || self == 0 || channelId < 0 || !std::isfinite(db)
		|| !manager->isListening(self, static_cast< unsigned int >(channelId))) {
		return;
	}
	const int whole = std::clamp(static_cast< int >(std::lround(db)), kListenMinDb, kListenMaxDb);
	const VolumeAdjustment adjustment = VolumeAdjustment::fromDBAdjustment(whole);
	const unsigned int channel = static_cast< unsigned int >(channelId);
	// shown at once; servers with the protobuf audio protocol apply it themselves (Mumble's ListenerVolumeSlider)
	manager->setListenerVolumeAdjustment(self, channel, adjustment);
	if (Global::get().sh && Global::get().sh->m_version >= Mumble::Protocol::PROTOBUF_INTRODUCTION_VERSION) {
		m_volumesToSend.insert(channel, adjustment.factor);
		m_volumeTimer->start();
	}
}

void DeskChannel::sendListeningVolumes() {
	if (!Global::get().sh || Global::get().uiSession == 0 || m_volumesToSend.isEmpty()) {
		m_volumesToSend.clear();
		return;
	}
	MumbleProto::UserState mpus;
	mpus.set_session(Global::get().uiSession);
	for (auto it = m_volumesToSend.cbegin(); it != m_volumesToSend.cend(); ++it) {
		MumbleProto::UserState::VolumeAdjustment *adjustment = mpus.add_listening_volume_adjustment();
		adjustment->set_listening_channel(it.key());
		adjustment->set_volume_adjustment(it.value());
	}
	m_volumesToSend.clear();
	Global::get().sh->sendMessage(mpus);
}

void DeskChannel::setListenQuieter(bool on) {
	if (!m_allowed()) {
		return;
	}
	// the user decided: the default must not undo it later
	QSettings().setValue(kQuieterDefaulted, true);
	setQuieter(on);
}

void DeskChannel::setQuieter(bool on) {
	Settings &s = Global::get().s;
	if (on == listenQuieter()) {
		return;
	}
	// on: Ruumble's strong attenuation (Mumble's audio settings can fine-tune it afterwards)
	s.listenerAttenuationFactor = on ? kQuieterFactor : 1.0f;
	if (!on) {
		s.alwaysAttenuateListeners = false;
	}
	s.save();
	emit listenQuieterChanged();
}

} // namespace ruumbledesk
