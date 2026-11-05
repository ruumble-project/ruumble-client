// Ruumble Client: connection and visitors of the City Map's addresses (ticket #8).
// The same UDP ping Mumble's own server list sends (ConnectDialog::sendPing), for a handful of addresses.

#ifndef MUMBLE_MUMBLE_RUUMBLE_SERVERPING_H_
#define MUMBLE_MUMBLE_RUUMBLE_SERVERPING_H_

#include "MumbleProtocol.h"

#include <QtCore/QElapsedTimer>
#include <QtCore/QHash>
#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtNetwork/QHostAddress>

class QTimer;
class QUdpSocket;

namespace ruumbledesk {

class ServerPing : public QObject {
	Q_OBJECT

public:
	explicit ServerPing(QObject *parent = nullptr);

	/** the addresses to ping ("host:port"); replaces the earlier set, keeps what is known of the same ones */
	void setTargets(const QStringList &targets);
	/** pings every two seconds while running */
	void start();
	void stop();

	struct Result {
		int ms        = -1; // round trip, -1: no answer (yet)
		int users     = -1;
		int maxUsers  = -1;
		bool answered = false;
		bool gaveUp   = false; // no answer for a while (or the name does not resolve)
	};
	Result result(const QString &target) const { return m_results.value(target); }

signals:
	/** a result changed */
	void updated();

private:
	struct Target {
		QString host;
		unsigned short port = 0;
		QList< QHostAddress > addresses; // resolved: every one is pinged, IPv4 and IPv6, as Mumble's list does
		bool resolving    = false;
		bool failed       = false;
		quint64 key       = 0; // hides the timestamp from the server, like Mumble does
		qint64 firstSent  = -1;
		qint64 lastAnswer = -1;
		bool logged       = false; // the first answer or the silence is logged once
	};

	void tick();
	void send(Target &target);
	static bool matches(const Target &target, const QHostAddress &host, quint16 port);
	void read();

	QUdpSocket *m_socket;
	QTimer *m_timer;
	QElapsedTimer m_clock;
	QHash< QString, Target > m_targets;
	QHash< QString, Result > m_results;
	Mumble::Protocol::UDPPingEncoder< Mumble::Protocol::Role::Client > m_encoder;
	Mumble::Protocol::UDPDecoder< Mumble::Protocol::Role::Client > m_decoder;
};

} // namespace ruumbledesk

#endif
