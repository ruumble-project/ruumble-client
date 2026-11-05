#include "ServerPing.h"

#include "Version.h"

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
		m_socket->bind(QHostAddress::Any); // dual stack: IPv4 and IPv6
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
		if (t.address.isNull()) {
			if (t.resolving) {
				continue;
			}
			QHostAddress literal;
			if (literal.setAddress(t.host)) {
				t.address = literal;
			} else {
				t.resolving = true;
				QHostInfo::lookupHost(t.host, this, [this, id](const QHostInfo &info) {
					auto found = m_targets.find(id);
					if (found == m_targets.end()) {
						return;
					}
					found->resolving = false;
					if (info.error() != QHostInfo::NoError || info.addresses().isEmpty()) {
						found->failed     = true;
						Result &r         = m_results[id];
						r.gaveUp          = true;
						emit updated();
						return;
					}
					// prefer IPv4 like most servers are reached
					found->address = info.addresses().first();
					for (const QHostAddress &a : info.addresses()) {
						if (a.protocol() == QAbstractSocket::IPv4Protocol) {
							found->address = a;
							break;
						}
					}
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
		m_socket->writeDatagram(reinterpret_cast< const char * >(packet.data()), static_cast< qint64 >(packet.size()),
								t.address, t.port);
	}
	if (t.firstSent < 0) {
		t.firstSent = m_clock.elapsed();
	}
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
			// IPv4 answers may come as IPv4-mapped IPv6 on a dual stack socket
			if (t.port != port || !t.address.isEqual(host, QHostAddress::TolerantConversion)) {
				continue;
			}
			const qint64 now = m_clock.elapsed();
			const qint64 sent = static_cast< qint64 >(data.timestamp ^ t.key);
			if (sent < 0 || sent > now) {
				continue; // not one of ours
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
