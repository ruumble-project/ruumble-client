// Ruumble Client: what the Ruumble web UI may use of the client, over QWebChannel as object "desk".
// Contract with web/src/lib/desk/ in ruumble-server (submodule server/):
//   properties  version (string, Mumble's), clientVersion (string, Ruumble Client's VERSION or "unknown"; 0.39.0 and
//               newer), server (string, "host:port" or empty while not connected)
//   signals     logged(entry), serverChanged()
//   methods     history() -> entry[], sendMessage(text) -> bool,
//               openSettings(), openConnect(), openAudioWizard(), disconnectServer(),
//               showMenu(x, y) (Mumble's menus at CSS pixel x/y of the page), showUserList(on),
//               showChannelMenu(channelId, x, y), showUserMenu(session, x, y) (Mumble's own context menus for a
//               channel or a user: add, remove, edit, ACL, links, kick, ban, ... ), moveUser(session, channelId),
//               channelProperties(channelId) -> { name, maxUsers, descriptionHash } | {} (unknown channel),
//               updateChannel(channelId, { name?, description?, maxUsers?, position? }) -> { sent, descriptionHash? }
//               (Ruumble's edit dialog, server#15: sends one ChannelState with the given fields; the server decides,
//               the page sees the result in channelProperties and the next snapshot. descriptionHash: SHA-1 hex of the
//               description as UTF-8, as Mumble keeps it)
//               createChannel(parentId, name) -> { sent }, removeChannel(channelId) -> { sent } (the rooms of the floor
//               care, client#12; Mumble checks the permissions and moves the people of a removed channel to its parent)
//   entry       { id: number, time: ms since epoch, kind: "text"|"private"|"own"|"warning"|"info", text: string }
// How the user transmits (ticket #9; all of it absent in older clients, the page checks for transmitMode):
//   properties  transmitMode ("vad"|"ptt"|"continuous"), transmitting (bool, the own talk state, from the client),
//               micLevel (0..1, only kept up to date while watchLevel(true) or while transmitting), latched (bool),
//               pttKey (key), pttOptions ({ doublePush: bool, audioCue: bool })
//   signals     transmitModeChanged(), transmittingChanged(), micLevelChanged(), latchedChanged(), pttKeyChanged(),
//               pttOptionsChanged(), keyCaptured(key)
//   methods     setTransmitMode(mode) -> bool, pushToTalk(down) (the page's press: mouse on the microphone, or the key
//               while scope is "window"), setLatched(on), watchLevel(on), captureKey() -> bool (listens for the next
//               key or mouse button, answer in keyCaptured), capturePageKey(codes) (a key the page saw while capturing),
//               cancelCapture(), acceptKey() -> bool (binds the captured key as Mumble's push-to-talk shortcut),
//               setPttOption(name, value) -> bool
//   key         { set: bool, label: string (Mumble's names, "Control_R"), codes: string[] (DOM codes such as
//               "ControlRight", or "MouseBack"/"MouseForward"/"MouseMiddle"; empty when one is unknown),
//               scope: "global"|"window" (window: the key works only while Ruumble Client has the focus, e.g. Wayland),
//               warn: bool (a key that types or acts in other programs too) }
// The client lets go of the page's presses by itself when the page reloads or leaves, the mode changes or it quits.
// Log texts are plain text (never Mumble's HTML), so the page can show them without parsing markup.

#ifndef MUMBLE_MUMBLE_RUUMBLE_DESKCHANNEL_H_
#define MUMBLE_MUMBLE_RUUMBLE_DESKCHANNEL_H_

#include "Log.h"

#include <QtCore/QObject>
#include <QtCore/QPoint>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QVariant>

#include <deque>
#include <functional>

class MainWindow;

namespace ruumbledesk {

class TransmitControl;

class DeskChannel : public QObject {
	Q_OBJECT
	Q_PROPERTY(QString version READ version CONSTANT)
	/** Ruumble Client's own version (VERSION), for the web UI's "a newer client is out" (ruumble-server#18) */
	Q_PROPERTY(QString clientVersion READ clientVersion CONSTANT)
	Q_PROPERTY(QString server READ server NOTIFY serverChanged)
	Q_PROPERTY(QString transmitMode READ transmitMode NOTIFY transmitModeChanged)
	Q_PROPERTY(bool transmitting READ transmitting NOTIFY transmittingChanged)
	Q_PROPERTY(double micLevel READ micLevel NOTIFY micLevelChanged)
	Q_PROPERTY(bool latched READ latched NOTIFY latchedChanged)
	Q_PROPERTY(QVariantMap pttKey READ pttKey NOTIFY pttKeyChanged)
	Q_PROPERTY(QVariantMap pttOptions READ pttOptions NOTIFY pttOptionsChanged)

public:
	/** allowed() says whether the page that calls is the Ruumble service (checked on every call) */
	DeskChannel(MainWindow *mw, std::function< bool() > allowed, std::function< void(const QPoint &) > showMenu,
				std::function< void(bool) > showUserList, QObject *parent);

	QString version() const;
	QString clientVersion() const;
	QString server() const { return m_server; }
	QString transmitMode() const;
	bool transmitting() const;
	double micLevel() const;
	bool latched() const;
	QVariantMap pttKey() const;
	QVariantMap pttOptions() const;

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
	/** a new permanent channel below `parentId` (a room on a floor) */
	Q_INVOKABLE QVariantMap createChannel(int parentId, const QString &name);
	/** removes a channel (the server moves its people to the parent channel) */
	Q_INVOKABLE QVariantMap removeChannel(int channelId);

	Q_INVOKABLE bool setTransmitMode(const QString &mode);
	Q_INVOKABLE void pushToTalk(bool down);
	Q_INVOKABLE void setLatched(bool on);
	Q_INVOKABLE void watchLevel(bool on);
	Q_INVOKABLE bool captureKey();
	Q_INVOKABLE void capturePageKey(const QStringList &codes);
	Q_INVOKABLE void cancelCapture();
	Q_INVOKABLE bool acceptKey();
	Q_INVOKABLE bool setPttOption(const QString &name, const QVariant &value);

	/** from the integration: the page reloads, leaves or crashed; lets go of everything it held */
	void releasePage();

	/** from the integration: connection to a Mumble server changed */
	void setServer(const QString &server);

signals:
	void logged(const QVariantMap &entry);
	void serverChanged();
	void transmitModeChanged();
	void transmittingChanged();
	void micLevelChanged();
	void latchedChanged();
	void pttKeyChanged();
	void pttOptionsChanged();
	void keyCaptured(const QVariantMap &key);
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
	TransmitControl *m_transmit;
	std::function< bool() > m_allowed;
	std::function< void(const QPoint &) > m_showMenu;
	std::function< void(bool) > m_showUserList;
	QString m_server;
	std::deque< QVariantMap > m_history;
	int m_nextId = 1;
};

} // namespace ruumbledesk

#endif
