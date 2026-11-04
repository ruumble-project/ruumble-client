// Ruumble Desk: what the Ruumble web UI may use of the client, over QWebChannel as object "desk".
// Contract with web/src/lib/desk/ in the Ruumble repo:
//   properties  version (string), server (string, "host:port" or empty while not connected)
//   signals     logged(entry), serverChanged()
//   methods     history() -> entry[], sendMessage(text) -> bool,
//               openSettings(), openConnect(), openAudioWizard(), disconnectServer(),
//               showMenu(x, y) (Mumble's menus at CSS pixel x/y of the page), showUserList(on)
//   entry       { id: number, time: ms since epoch, kind: "text"|"private"|"own"|"warning"|"info", text: string }
// Log texts are plain text (never Mumble's HTML), so the page can show them without parsing markup.

#ifndef MUMBLE_MUMBLE_RUUMBLE_DESKCHANNEL_H_
#define MUMBLE_MUMBLE_RUUMBLE_DESKCHANNEL_H_

#include "Log.h"

#include <QtCore/QObject>
#include <QtCore/QPoint>
#include <QtCore/QString>
#include <QtCore/QVariant>

#include <deque>
#include <functional>

class MainWindow;

namespace ruumbledesk {

class DeskChannel : public QObject {
	Q_OBJECT
	Q_PROPERTY(QString version READ version CONSTANT)
	Q_PROPERTY(QString server READ server NOTIFY serverChanged)

public:
	/** allowed() says whether the page that calls is the Ruumble service (checked on every call) */
	DeskChannel(MainWindow *mw, std::function< bool() > allowed, std::function< void(const QPoint &) > showMenu,
				std::function< void(bool) > showUserList, QObject *parent);

	QString version() const;
	QString server() const { return m_server; }

	/** also tells the integration that the page has the Desk panel (panelSeen) */
	Q_INVOKABLE QVariantList history();
	Q_INVOKABLE bool sendMessage(const QString &text);
	Q_INVOKABLE void openSettings();
	Q_INVOKABLE void openConnect();
	Q_INVOKABLE void openAudioWizard();
	Q_INVOKABLE void disconnectServer();
	Q_INVOKABLE void showMenu(double x, double y);
	Q_INVOKABLE void showUserList(bool on);

	/** from the integration: connection to a Mumble server changed */
	void setServer(const QString &server);

signals:
	void logged(const QVariantMap &entry);
	void serverChanged();
	/** the page asked for the log: it has the Desk panel (web UI 0.33.0 or newer) */
	void panelSeen();

private:
	void attachLog();
	void onLogged(Log::MsgType type, const QString &plain, bool own);

	MainWindow *m_mw;
	std::function< bool() > m_allowed;
	std::function< void(const QPoint &) > m_showMenu;
	std::function< void(bool) > m_showUserList;
	QString m_server;
	std::deque< QVariantMap > m_history;
	int m_nextId = 1;
};

} // namespace ruumbledesk

#endif
