#include "CityMap.h"

#include "ServerPing.h"

#include "Database.h"
#include "Global.h"
#include "MainWindow.h"
#include "Mumble.pb.h"
#include "ServerHandler.h"

#include <QtCore/QFile>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QLocale>
#include <QtCore/QTimer>
#include <QtCore/QUrl>
#include <QtCore/QUrlQuery>
#include <QtNetwork/QAbstractSocket>
#include <QtNetwork/QHostAddress>
#include <QtWidgets/QApplication>

namespace ruumbledesk {

namespace {
	constexpr int kTickMs     = 500;
	constexpr int kLimitMs    = 15000; // ticket #8: give up entering after 15 s
	constexpr int kStoppedMax = 3;     // ticks without a running connection before it counts as failed

	QString ownSettingsPath() { return Global::get().qdBasePath.filePath(QStringLiteral("city-map.json")); }

	QString target(const FavoriteServer &fs) { return QStringLiteral("%1:%2").arg(fs.qsHostname).arg(fs.usPort); }

	bool sameAddress(const QString &host, unsigned short port, const QString &user, const FavoriteServer &fs) {
		return fs.qsHostname.compare(host, Qt::CaseInsensitive) == 0 && fs.usPort == port && fs.qsUsername == user;
	}

	/** the language Mumble uses (settings, else the system), as the page knows it: "de" or "en" */
	QString language() {
		QLocale locale(Global::get().s.qsLanguage);
		if (locale == QLocale::c()) {
			locale = QLocale::system();
		}
		return locale.language() == QLocale::German ? QStringLiteral("de") : QStringLiteral("en");
	}

	bool connectedTo(const FavoriteServer &fs) {
		if (!Global::get().sh || !Global::get().sh->isRunning() || Global::get().uiSession == 0) {
			return false;
		}
		QString host, user, pw;
		unsigned short port = 0;
		Global::get().sh->getConnectionInfo(host, port, user, pw);
		return sameAddress(host, port, user, fs);
	}
} // namespace

CityMap::CityMap(MainWindow *mw, std::function< bool() > allowed, std::function< void() > show,
				 std::function< void() > leave, QObject *parent)
	: QObject(parent), m_mw(mw), m_allowed(std::move(allowed)), m_show(std::move(show)), m_leave(std::move(leave)),
	  m_ping(new ServerPing(this)), m_entryTimer(new QTimer(this)) {
	m_entryTimer->setInterval(kTickMs);
	QObject::connect(m_entryTimer, &QTimer::timeout, this, &CityMap::tickEntry);
	QObject::connect(m_ping, &ServerPing::updated, this, &CityMap::sendPing);
	loadOwnSettings();
}

CityMap::~CityMap() = default;

// ---------------------------------------------------------------- own settings

void CityMap::loadOwnSettings() {
	QFile file(ownSettingsPath());
	if (!file.open(QIODevice::ReadOnly)) {
		return;
	}
	const QJsonObject o = QJsonDocument::fromJson(file.readAll()).object();
	m_hq                = o.value(QStringLiteral("nextVisit")).toString() == QLatin1String("hq");
	const QJsonObject hq = o.value(QStringLiteral("headquarters")).toObject();
	m_hqHost             = hq.value(QStringLiteral("host")).toString();
	m_hqPort             = static_cast< unsigned short >(hq.value(QStringLiteral("port")).toInt());
	m_hqUser             = hq.value(QStringLiteral("user")).toString();
}

void CityMap::saveOwnSettings() const {
	QJsonObject hq;
	hq.insert(QStringLiteral("host"), m_hqHost);
	hq.insert(QStringLiteral("port"), m_hqPort);
	hq.insert(QStringLiteral("user"), m_hqUser);
	QJsonObject o;
	o.insert(QStringLiteral("nextVisit"), m_hq ? QStringLiteral("hq") : QStringLiteral("other"));
	o.insert(QStringLiteral("headquarters"), hq);
	QFile file(ownSettingsPath());
	if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		file.write(QJsonDocument(o).toJson());
	}
}

// ---------------------------------------------------------------- addresses and choice

QList< FavoriteServer > CityMap::favorites() const {
	return Global::get().db->getFavorites(); // ordered by name; the index is the page's id
}

void CityMap::store(const QList< FavoriteServer > &list) {
	Global::get().db->setFavorites(list);
}

int CityMap::headquarters(const QList< FavoriteServer > &list) const {
	if (m_hqHost.isEmpty()) {
		return -1;
	}
	for (int i = 0; i < list.size(); ++i) {
		if (sameAddress(m_hqHost, m_hqPort, m_hqUser, list.at(i))) {
			return i;
		}
	}
	return -1;
}

void CityMap::setHeadquarters(const FavoriteServer *fs) {
	m_hqHost = fs ? fs->qsHostname : QString();
	m_hqPort = fs ? fs->usPort : 0;
	m_hqUser = fs ? fs->qsUsername : QString();
}

QString CityMap::mode() const {
	if (!Global::get().s.bAutoConnect) {
		return QStringLiteral("never");
	}
	if (m_hq) {
		// headquarters gone (removed in Mumble's own list, for example): no building is entered automatically
		return headquarters(favorites()) >= 0 ? QStringLiteral("hq") : QStringLiteral("never");
	}
	return QStringLiteral("last");
}

// ---------------------------------------------------------------- Mumble's side

bool CityMap::open(bool autoconnect) {
	if (m_bypass) {
		m_bypass = false;
		return false;
	}
	if (m_hq && headquarters(favorites()) < 0) {
		// headquarters was removed elsewhere (Mumble's own list): "Do not enter any building automatically"
		m_hq                         = false;
		Global::get().s.bAutoConnect = false;
		Global::get().s.save();
		setHeadquarters(nullptr);
		saveOwnSettings();
	}
	const bool first = autoconnect && !m_autoTried;
	m_autoTried      = true;
	if (first) {
		const QString how                = mode();
		const QList< FavoriteServer > list = favorites();
		if (how == QLatin1String("hq")) {
			startEntry(list.at(headquarters(list)), true);
			return true;
		}
		if (how == QLatin1String("last")) {
			for (const FavoriteServer &fs : list) {
				if (fs.qsName == Global::get().s.qsLastServer) {
					startEntry(fs, true);
					return true;
				}
			}
		}
	}
	m_note = Note();
	showMap();
	return true;
}

bool CityMap::connectionFailed(int socketError, const QString &reason, int rejectType) {
	if (m_swallowNext) {
		m_swallowNext = false;
		return true;
	}
	if (!m_entry.active) {
		return false;
	}
	switch (rejectType) {
		case MumbleProto::Reject_RejectType_UsernameInUse:
			fail(QStringLiteral("nameInUse"));
			return true;
		case MumbleProto::Reject_RejectType_WrongUserPW:
		case MumbleProto::Reject_RejectType_WrongServerPW:
		case MumbleProto::Reject_RejectType_InvalidUsername:
			fail(QStringLiteral("refused"));
			return true;
		default:
			break;
	}
	if (m_entry.timedOut) {
		failAfterLimit();
		return true;
	}
	if (socketError == QAbstractSocket::SslHandshakeFailedError) {
		fail(QStringLiteral("tls"), reason);
		return true;
	}
	// The server or one of its addresses did not answer: Mumble tries again (its reconnect), the entering goes on
	// until it succeeds or the 15 s run out (ticket #8). Mumble only logs this attempt.
	m_entry.lastReason = reason;
	return false;
}

void CityMap::failAfterLimit() {
	if (m_entry.lastReason.isEmpty()) {
		fail(QStringLiteral("timeout"));
	} else {
		fail(QStringLiteral("unreachable"), m_entry.lastReason);
	}
}

void CityMap::entered() {
	if (m_entry.active) {
		m_entry = Entry();
		m_entryTimer->stop();
		m_note = Note();
		notify();
	}
}

void CityMap::pageLeft() {
	m_shown = false;
	m_ping->stop();
}

// ---------------------------------------------------------------- entering

void CityMap::startEntry(const FavoriteServer &fs, bool automatic) {
	m_note      = Note();
	m_lastTried = fs.qsName;
	if (connectedTo(fs)) {
		// already in this building: back to it
		m_shown = false;
		m_leave();
		return;
	}
	QUrl url;
	url.setScheme(QStringLiteral("mumble"));
	url.setHost(fs.qsHostname);
	url.setPort(fs.usPort);
	url.setUserName(fs.qsUsername);
	url.setPassword(fs.qsPassword);
	QUrlQuery query;
	query.addQueryItem(QStringLiteral("title"), fs.qsName); // becomes Mumble's "last server"
	url.setQuery(query);

	showMap(); // the entering is shown on the City Map page
	m_entry = Entry();
	// openUrl first: it ends a running connection and handles that disconnect right away, which must not count as a
	// failure of this entering
	m_mw->openUrl(url);
	m_entry.active    = true;
	m_entry.automatic = automatic;
	m_entry.name      = fs.qsName;
	m_entry.host      = fs.qsHostname;
	m_entry.port      = fs.usPort;
	m_entry.user      = fs.qsUsername;
	m_stoppedTicks    = 0;
	m_entryTimer->start();
	notify();
}

void CityMap::tickEntry() {
	if (!m_entry.active) {
		m_entryTimer->stop();
		return;
	}
	// a question of Mumble is open (certificate): the time stands still
	if (QApplication::activeModalWidget()) {
		return;
	}
	const bool running = Global::get().sh && Global::get().sh->isRunning();
	if (m_entry.timedOut) {
		// waiting for the disconnect asked for below; normally connectionFailed comes first
		if (!running) {
			failAfterLimit();
		}
		return;
	}
	if (running) {
		m_entry.started = true;
		m_stoppedTicks  = 0;
	} else if (m_entry.started && Global::get().sh && !Global::get().sh->qlErrors.isEmpty()
			   && ++m_stoppedTicks >= kStoppedMax) {
		// ended after the certificate question without a new attempt: the certificate was not accepted
		fail(QStringLiteral("tls"));
		return;
	}
	m_entry.elapsedMs += kTickMs;
	if (m_entry.elapsedMs >= kLimitMs) {
		m_entry.timedOut = true;
		// also stops Mumble's reconnect timer; a running attempt comes back to connectionFailed
		m_mw->disconnectFromServer();
		if (!running) {
			failAfterLimit();
		}
		return;
	}
	notify();
}

void CityMap::fail(const QString &reason, const QString &detail) {
	m_note.building = m_entry.name;
	m_note.reason   = reason;
	m_note.detail   = detail;
	m_entry         = Entry();
	m_entryTimer->stop();
	m_stoppedTicks = 0;
	showMap();
	notify();
}

void CityMap::showMap() {
	if (!m_shown) {
		m_shown = true;
		m_show();
	}
}

// ---------------------------------------------------------------- the page

QVariantMap CityMap::state() {
	if (!m_allowed()) {
		return {};
	}
	const QList< FavoriteServer > list = favorites();
	const int hq                       = headquarters(list);
	QVariantList accounts;
	for (int i = 0; i < list.size(); ++i) {
		const FavoriteServer &fs = list.at(i);
		accounts << QVariantMap{ { QStringLiteral("id"), i },
								 { QStringLiteral("name"), fs.qsName },
								 { QStringLiteral("host"), fs.qsHostname },
								 { QStringLiteral("port"), fs.usPort },
								 { QStringLiteral("user"), fs.qsUsername },
								 { QStringLiteral("hasCode"), !fs.qsPassword.isEmpty() },
								 { QStringLiteral("last"), fs.qsName == Global::get().s.qsLastServer } };
	}
	QVariantMap s{ { QStringLiteral("lang"), language() },
				   { QStringLiteral("accounts"), accounts },
				   { QStringLiteral("nextVisit"), mode() },
				   { QStringLiteral("headquarters"), hq } };
	if (!m_note.building.isEmpty()) {
		s.insert(QStringLiteral("note"), QVariantMap{ { QStringLiteral("building"), m_note.building },
													  { QStringLiteral("reason"), m_note.reason },
													  { QStringLiteral("detail"), m_note.detail } });
	}
	if (m_entry.active) {
		s.insert(QStringLiteral("entering"),
				 QVariantMap{ { QStringLiteral("name"), m_entry.name },
							  { QStringLiteral("host"), m_entry.host },
							  { QStringLiteral("automatic"), m_entry.automatic },
							  { QStringLiteral("mode"), m_entry.automatic ? mode() : QString() },
							  { QStringLiteral("seconds"), m_entry.elapsedMs / 1000 },
							  { QStringLiteral("limit"), kLimitMs / 1000 } });
	}
	return s;
}

QString CityMap::save(int id, const QVariantMap &entry) {
	if (!m_allowed()) {
		return QStringLiteral("denied");
	}
	const QString name = entry.value(QStringLiteral("name")).toString().trimmed();
	const QString host = entry.value(QStringLiteral("host")).toString().trimmed();
	const QString user = entry.value(QStringLiteral("user")).toString().trimmed();
	bool portOk        = false;
	const uint port    = entry.value(QStringLiteral("port")).toString().trimmed().toUInt(&portOk);
	if (name.isEmpty()) {
		return QStringLiteral("name");
	}
	if (host.isEmpty() || host.contains(QLatin1Char(' ')) || host.contains(QLatin1Char('/'))) {
		return QStringLiteral("host");
	}
	if (!portOk || port < 1 || port > 65535) {
		return QStringLiteral("port");
	}
	if (user.isEmpty()) {
		return QStringLiteral("user");
	}

	QList< FavoriteServer > list = favorites();
	if (id >= list.size()) {
		return QStringLiteral("gone");
	}
	FavoriteServer fs;
	if (id >= 0) {
		fs = list.at(id);
	}
	const bool wasHq   = id >= 0 && headquarters(list) == id;
	const bool wasLast = id >= 0 && fs.qsName == Global::get().s.qsLastServer;
	fs.qsName          = name;
	fs.qsHostname      = host;
	fs.usPort          = static_cast< unsigned short >(port);
	fs.qsUsername      = user;
	if (id < 0 || entry.value(QStringLiteral("codeChanged")).toBool()) {
		fs.qsPassword = entry.value(QStringLiteral("code")).toString();
	}
	if (id >= 0) {
		list[id] = fs;
	} else {
		list << fs;
	}
	store(list);
	if (wasHq) {
		// headquarters follows the edited address
		setHeadquarters(&fs);
		saveOwnSettings();
	}
	if (wasLast) {
		Global::get().s.qsLastServer = fs.qsName;
	}
	updatePing();
	notify();
	return QString();
}

void CityMap::remove(int id) {
	if (!m_allowed()) {
		return;
	}
	QList< FavoriteServer > list = favorites();
	if (id < 0 || id >= list.size()) {
		return;
	}
	if (headquarters(list) == id) {
		// ticket #8: removing headquarters falls back to "Do not enter any building automatically"
		setHeadquarters(nullptr);
		if (m_hq) {
			m_hq                         = false;
			Global::get().s.bAutoConnect = false;
			Global::get().s.save();
		}
		saveOwnSettings();
	}
	list.removeAt(id);
	store(list);
	updatePing();
	notify();
}

void CityMap::setNextVisit(const QString &how, int id) {
	if (!m_allowed()) {
		return;
	}
	const QList< FavoriteServer > list = favorites();
	if (how == QLatin1String("never")) {
		Global::get().s.bAutoConnect = false;
		m_hq                         = false;
	} else if (how == QLatin1String("last")) {
		Global::get().s.bAutoConnect = true;
		m_hq                         = false;
	} else if (how == QLatin1String("hq") && id >= 0 && id < list.size()) {
		Global::get().s.bAutoConnect = true;
		m_hq                         = true;
		setHeadquarters(&list.at(id));
	} else {
		return;
	}
	Global::get().s.save();
	saveOwnSettings();
	notify();
}

void CityMap::enter(int id) {
	if (!m_allowed()) {
		return;
	}
	const QList< FavoriteServer > list = favorites();
	if (id >= 0 && id < list.size()) {
		startEntry(list.at(id), false);
	}
}

void CityMap::tryAgain() {
	if (!m_allowed()) {
		return;
	}
	for (const FavoriteServer &fs : favorites()) {
		if (fs.qsName == m_lastTried) {
			startEntry(fs, false);
			return;
		}
	}
}

void CityMap::cancel() {
	if (!m_allowed()) {
		return;
	}
	if (m_entry.active) {
		// cancel the entering: back to the City Map
		m_entry = Entry();
		m_entryTimer->stop();
		m_stoppedTicks = 0;
		m_swallowNext  = true;
		QTimer::singleShot(3000, this, [this] { m_swallowNext = false; });
		m_mw->disconnectFromServer();
		notify();
		return;
	}
	m_note  = Note();
	m_shown = false;
	m_ping->stop();
	m_leave();
}

void CityMap::ready() {
	if (!m_allowed()) {
		return;
	}
	m_shown = true;
	updatePing();
	m_ping->start();
	notify();
}

void CityMap::updatePing() {
	QStringList targets;
	for (const FavoriteServer &fs : favorites()) {
		targets << target(fs);
	}
	m_ping->setTargets(targets);
	sendPing();
}

void CityMap::sendPing() {
	if (!m_shown || !m_allowed()) {
		return; // never to another page
	}
	QVariantList out;
	const QList< FavoriteServer > list = favorites();
	for (int i = 0; i < list.size(); ++i) {
		const ServerPing::Result r = m_ping->result(target(list.at(i)));
		out << QVariantMap{ { QStringLiteral("id"), i },
							{ QStringLiteral("ms"), r.ms },
							{ QStringLiteral("users"), r.users },
							{ QStringLiteral("maxUsers"), r.maxUsers },
							{ QStringLiteral("answered"), r.answered },
							{ QStringLiteral("gaveUp"), r.gaveUp },
							{ QStringLiteral("via"), r.via } };
	}
	emit pinged(out);
}

void CityMap::notify() {
	if (m_allowed()) {
		emit changed();
	}
}

} // namespace ruumbledesk
