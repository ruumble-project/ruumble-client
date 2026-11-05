#include "ServerPing.h"

#include "Version.h"

#include <QtCore/QDebug>
#include <QtCore/QRandomGenerator>
#include <QtCore/QTimer>
#include <QtNetwork/QHostInfo>
#include <QtNetwork/QUdpSocket>

namespace ruumbledesk {

namespace {
	constexpr int kIntervalMs = 2000;
	constexpr qint64 kGiveUpMs = 6000; // no answer for this long: shown as "does not answer"
} // namespace

ServerPing::ServerPing(QObject *parent) : QObject(parent), m_socket(new QUdpSocket(this)), m_timer(new QTimer(this)) {
	m_clock.start();
	m_timer->setInterval(kIntervalMs);
	QObject::connect(m_timer, &QTimer::timeout, this, &ServerPing::tick);
	QObject::connect(m_socket, &QUdpSocket::readyRead, this, &ServerPing::read);
}

void ServerPing::setTargets(const QStringList &targets) {
	QHash< QString, Target > next;
	for (const QString &id : targets) {
		if (m_targets.contains(id)) {
			next.insert(id, m_targets.value(id));
			continue;
		}
		const qsizetype colon = id.lastIndexOf(QLatin1Char(':'));
		Target t;
		t.host = id.left(colon);
		t.port = static_cast< unsigned short >(id.mid(colon + 1).toUInt());
		t.key  = QRandomGenerator::global()->generate64() << 32;
		next.insert(id, t);
	}
	m_targets = next;
	for (auto it = m_results.begin(); it != m_results.end();) {
		it = m_targets.contains(it.key()) ? std::next(it) : m_results.erase(it);
	}
	if (m_timer->isActive()) {
		tick();
	}
}

void ServerPing::start() {
	if (m_socket->state() != QAbstractSocket::BoundState) {
		// dual stack: IPv4 and IPv6
		if (!m_socket->bind(QHostAddress::Any)) {
			qWarning().noquote() << "Ruumble City Map: ping socket not bound:" << m_socket->errorString();
		}
	}
	m_timer->start();
	tick();
}

void ServerPing::stop() {
	m_timer->stop();
}

void ServerPing::tick() {
	bool changed = false;
	for (auto it = m_targets.begin(); it != m_targets.end(); ++it) {
		Target &t     = it.value();
		const QString id = it.key();
		if (t.failed) {
			continue;
		}
		if (t.addresses.isEmpty()) {
			if (t.resolving) {
				continue;
			}
			QHostAddress literal;
			if (literal.setAddress(t.host)) {
				t.addresses << literal;
			} else {
				t.resolving = true;
				QHostInfo::lookupHost(t.host, this, [this, id](const QHostInfo &info) {
					auto found = m_targets.find(id);
					if (found == m_targets.end()) {
						return;
					}
					found->resolving = false;
					if (info.error() != QHostInfo::NoError || info.addresses().isEmpty()) {
						qInfo().noquote() << "Ruumble City Map: ping" << id << "- name not resolved:" << info.errorString();
						found->failed     = true;
						Result &r         = m_results[id];
						r.gaveUp          = true;
						emit updated();
						return;
					}
					// all of them: a server may answer on IPv6 only (or IPv4 only), Mumble's list pings every address too
					found->addresses = info.addresses();
					QStringList list;
					for (const QHostAddress &a : found->addresses) {
						list << a.toString();
					}
					qInfo().noquote() << "Ruumble City Map: ping" << id << "to" << list.join(QLatin1String(", "));
					send(*found);
				});
				continue;
			}
		}
		send(t);
		Result &r = m_results[id];
		const qint64 now = m_clock.elapsed();
		const qint64 since = t.lastAnswer >= 0 ? t.lastAnswer : t.firstSent;
		const bool gaveUp = since >= 0 && now - since > kGiveUpMs;
		if (gaveUp && !t.logged) {
			t.logged = true;
			qInfo().noquote() << "Ruumble City Map: ping" << id << "- no answer for" << kGiveUpMs / 1000 << "s";
		}
		if (gaveUp != r.gaveUp) {
			r.gaveUp = gaveUp;
			if (gaveUp) {
				r.ms = -1;
			}
			changed = true;
		}
	}
	if (changed) {
		emit updated();
	}
}

void ServerPing::send(Target &t) {
	Mumble::Protocol::PingData data;
	data.timestamp                    = static_cast< quint64 >(m_clock.elapsed()) ^ t.key;
	data.requestAdditionalInformation = true;
	// old format first (understood by every server), then the new one for servers that only know that (as Mumble does)
	for (Version::full_t version : { Version::UNKNOWN, Mumble::Protocol::PROTOBUF_INTRODUCTION_VERSION }) {
		m_encoder.setProtocolVersion(version);
		const auto packet = m_encoder.encodePingPacket(data);
		for (const QHostAddress &address : t.addresses) {
			m_socket->writeDatagram(reinterpret_cast< const char * >(packet.data()),
									static_cast< qint64 >(packet.size()), address, t.port);
		}
	}
	if (t.firstSent < 0) {
		t.firstSent = m_clock.elapsed();
	}
}

bool ServerPing::matches(const Target &t, const QHostAddress &host, quint16 port) {
	if (t.port != port) {
		return false;
	}
	for (const QHostAddress &address : t.addresses) {
		// IPv4 answers may come as IPv4-mapped IPv6 on a dual stack socket; a scope id "0" means none (as in Mumble)
		QHostAddress from(host);
		if (from.scopeId() == QLatin1String("0")) {
			from.setScopeId(QString());
		}
		if (address.isEqual(from, QHostAddress::TolerantConversion)) {
			return true;
		}
	}
	return false;
}

void ServerPing::read() {
	bool changed = false;
	while (m_socket->hasPendingDatagrams()) {
		QHostAddress host;
		quint16 port = 0;
		auto buffer  = m_decoder.getBuffer();
		const qint64 len =
			m_socket->readDatagram(reinterpret_cast< char * >(buffer.data()), static_cast< qint64 >(buffer.size()), &host, &port);
		if (len <= 0) {
			continue;
		}
		m_decoder.setProtocolVersion(Version::UNKNOWN); // decodes both formats (see ConnectDialog::udpReply)
		if (!m_decoder.decodePing(buffer.subspan(0, static_cast< std::size_t >(len)))
			|| m_decoder.getMessageType() != Mumble::Protocol::UDPMessageType::Ping) {
			continue;
		}
		const Mumble::Protocol::PingData data = m_decoder.getPingData();
		for (auto it = m_targets.begin(); it != m_targets.end(); ++it) {
			Target &t = it.value();
			if (!matches(t, host, port)) {
				continue;
			}
			const qint64 now = m_clock.elapsed();
			const qint64 sent = static_cast< qint64 >(data.timestamp ^ t.key);
			if (sent < 0 || sent > now) {
				continue; // not one of ours
			}
			if (!t.logged) {
				t.logged = true;
				qInfo().noquote() << "Ruumble City Map: ping" << it.key() << "- answer from" << host.toString() << "in"
								  << (now - sent) << "ms," << data.userCount << "/" << data.maxUserCount << "visitors";
			}
			t.lastAnswer = now;
			Result &r    = m_results[it.key()];
			r.ms         = static_cast< int >(now - sent);
			r.users      = static_cast< int >(data.userCount);
			r.maxUsers   = static_cast< int >(data.maxUserCount);
			r.answered   = true;
			r.gaveUp     = false;
			changed      = true;
		}
	}
	if (changed) {
		emit updated();
	}
}

} // namespace ruumbledesk
