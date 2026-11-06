// Ruumble Client: what the Ruumble web UI may use of the client, over QWebChannel as object "desk".
// Contract with web/src/lib/desk/ in ruumble-server (submodule server/):
//   properties  version (string), server (string, "host:port" or empty while not connected)
//   signals     logged(entry), serverChanged()
//   methods     history() -> entry[], sendMessage(text) -> bool,
//               openSettings(), openConnect(), openAudioWizard(), disconnectServer(),
//               showMenu(x, y) (Mumble's menus at CSS pixel x/y of the page), showUserList(on),
//               showChannelMenu(channelId, x, y), showUserMenu(session, x, y) (Mumble's own context menus for a
//               channel or a user: add, remove, edit, ACL, links, kick, ban, ... ), moveUser(session, channelId),
//               channelProperties(channelId) -> { name, maxUsers, descriptionHash } | {} (unknown channel),
//               updateChannel(channelId, { name?, description?, maxUsers? }) -> { sent, descriptionHash? }
//               (Ruumble's edit dialog, server#15: sends one ChannelState with the given fields; the server decides,
//               the page sees the result in channelProperties and the next snapshot. descriptionHash: SHA-1 hex of the
//               description as UTF-8, as Mumble keeps it)
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
	/** Mumble's channel menu for that channel (ids as in the snapshot = Mumble's channel ids) */
	Q_INVOKABLE void showChannelMenu(int channelId, double x, double y);
	/** Mumble's user menu for that user (session as in the snapshot) */
	Q_INVOKABLE void showUserMenu(int session, double x, double y);
	/** drag and drop of a person onto a room: moves that user, with the user's own Mumble permissions */
	Q_INVOKABLE void moveUser(int session, int channelId);
	/** name, maximum users and the description's hash of a channel as this client knows it; {} if unknown */
	Q_INVOKABLE QVariantMap channelProperties(int channelId);
	/** changes name, description (stored as given, never turned into HTML) and maximum users of a channel */
	Q_INVOKABLE QVariantMap updateChannel(int channelId, const QVariantMap &changes);

	/** from the integration: connection to a Mumble server changed */
	void setServer(const QString &server);

signals:
	void logged(const QVariantMap &entry);
	void serverChanged();
	/** the page asked for the log: it has the Desk panel (web UI 0.33.0 or newer) */
	void panelSeen();
	// to the integration, queued: a nested menu loop must not run inside the web channel's message handler
	void channelMenuRequested(unsigned int channelId, const QPoint &cssPos);
	void userMenuRequested(unsigned int session, const QPoint &cssPos);
	void userMoveRequested(unsigned int session, unsigned int channelId);

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
