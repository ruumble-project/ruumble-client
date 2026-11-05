#include "ServerPing.h"

#include "ServerResolver.h"
#include "Version.h"

#include <QtCore/QDebug>
#include <QtCore/QRandomGenerator>
#include <QtCore/QTimer>
#include <QtNetwork/QUdpSocket>

namespace ruumbledesk {

namespace {
	constexpr int kIntervalMs = 2000;
	constexpr qint64 kGiveUpMs = 6000; // no answer for this long: shown as "does not answer"
} // namespace

ServerPing::ServerPing(QObject *parent)
	: QObject(parent), m_socket4(new QUdpSocket(this)), m_socket6(new QUdpSocket(this)), m_timer(new QTimer(this)) {
	m_clock.start();
	m_timer->setInterval(kIntervalMs);
	QObject::connect(m_timer, &QTimer::timeout, this, &ServerPing::tick);
	QObject::connect(m_socket4, &QUdpSocket::readyRead, this, [this] { read(m_socket4); });
	QObject::connect(m_socket6, &QUdpSocket::readyRead, this, [this] { read(m_socket6); });
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
	if (!m_bound4) {
		m_bound4 = m_socket4->bind(QHostAddress(QHostAddress::AnyIPv4), 0);
		if (!m_bound4) {
			qWarning().noquote() << "Ruumble City Map: IPv4 ping socket not bound:" << m_socket4->errorString();
		}
	}
	if (!m_bound6) {
		m_bound6 = m_socket6->bind(QHostAddress(QHostAddress::AnyIPv6), 0);
		if (!m_bound6) {
			qInfo().noquote() << "Ruumble City Map: IPv6 ping socket not bound:" << m_socket6->errorString();
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
		if (t.endpoints.isEmpty()) {
			if (!t.resolving) {
				t.resolving              = true;
				ServerResolver *resolver = new ServerResolver(this);
				QObject::connect(resolver, &ServerResolver::resolved, this, [this, id, resolver] {
					resolver->deleteLater();
					auto found = m_targets.find(id);
					if (found == m_targets.end()) {
						return;
					}
					found->resolving = false;
					QStringList list;
					for (ServerResolverRecord record : resolver->records()) {
						for (const HostAddress &address : record.addresses()) {
							found->endpoints << qMakePair(address.toAddress(), record.port());
							list << QStringLiteral("%1 port %2").arg(address.toAddress().toString()).arg(record.port());
						}
					}
					if (found->endpoints.isEmpty()) {
						qInfo().noquote() << "Ruumble City Map: ping" << id << "- name not resolved";
						found->failed        = true;
						m_results[id].gaveUp = true;
						emit updated();
						return;
					}
					qInfo().noquote() << "Ruumble City Map: ping" << id << "to" << list.join(QLatin1String(", "));
					send(*found);
				});
				resolver->resolve(t.host, t.port);
			}
			continue;
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
		for (const auto &endpoint : t.endpoints) {
			const bool v4 = endpoint.first.protocol() == QAbstractSocket::IPv4Protocol;
			QUdpSocket *socket = v4 ? m_socket4 : m_socket6;
			if (v4 ? !m_bound4 : !m_bound6) {
				continue;
			}
			socket->writeDatagram(reinterpret_cast< const char * >(packet.data()), static_cast< qint64 >(packet.size()),
								  endpoint.first, endpoint.second);
		}
	}
	if (t.firstSent < 0) {
		t.firstSent = m_clock.elapsed();
	}
}

bool ServerPing::matches(const Target &t, const QHostAddress &host, quint16 port) {
	// a scope id "0" means none (as in Mumble's ConnectDialog::udpReply)
	QHostAddress from(host);
	if (from.scopeId() == QLatin1String("0")) {
		from.setScopeId(QString());
	}
	for (const auto &endpoint : t.endpoints) {
		if (endpoint.second == port && endpoint.first.isEqual(from, QHostAddress::TolerantConversion)) {
			return true;
		}
	}
	return false;
}

void ServerPing::read(QUdpSocket *socket) {
	bool changed = false;
	while (socket->hasPendingDatagrams()) {
		QHostAddress host;
		quint16 port = 0;
		auto buffer  = m_decoder.getBuffer();
		const qint64 len =
			socket->readDatagram(reinterpret_cast< char * >(buffer.data()), static_cast< qint64 >(buffer.size()), &host, &port);
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
