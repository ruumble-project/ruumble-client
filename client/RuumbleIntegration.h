// Ruumble Client: Ruumble built into the client (ADR-0024, phases 0 and 1).
// Runs Ruumble's core like the plugin does and shows the web UI as the main view.
// The entry points from Mumble code are the constructor (MainWindow::setupGui), attachServerHandler (MainWindow, where
// a ServerHandler is created), openCityMap (MainWindow::openServerConnectDialog) and connectionFailed
// (MainWindow::serverDisconnected).

#ifndef MUMBLE_MUMBLE_RUUMBLE_RUUMBLEINTEGRATION_H_
#define MUMBLE_MUMBLE_RUUMBLE_RUUMBLEINTEGRATION_H_

#include <QtCore/QObject>
#include <QtCore/QString>

#include <memory>

class MainWindow;
class QDockWidget;
class QMenu;
class QPoint;
class ServerHandler;

namespace ruumble {
class Core;
class WebSocketTransport;
} // namespace ruumble

namespace ruumbledesk {

class CityMap;
class DeskChannel;
class QtMumbleApi;
class ObservingTransport;
class RuumbleView;

class RuumbleIntegration : public QObject {
	Q_OBJECT

public:
	/** places the view in mw (user tree moves into a dock) and starts the core */
	explicit RuumbleIntegration(MainWindow *mw);
	~RuumbleIntegration() override;

	/** call for every new ServerHandler (one per connection) */
	void attachServerHandler(ServerHandler *sh);

	/**
	 * The Ruumble City Map instead of Mumble's connection dialog (ticket #8). autoconnect: the call at start, which
	 * enters the building of "On your next visit". Returns false when Mumble's own dialog is to open (chosen in the
	 * Mumble menu).
	 */
	bool openCityMap(bool autoconnect);
	/**
	 * A connection ended with an error (not the certificate question). While a building is being entered from the City
	 * Map, the City Map shows the failure: returns true, and Mumble then shows no dialog and does not reconnect.
	 */
	bool connectionFailed(int socketError, const QString &reason, int rejectType);

	/** hides Mumble's menu bar, toolbar and docks: the Ruumble UI hosts their functions (MainWindow::setupView) */
	void applyChrome();
	/** window title from the page ("Lobby · Ruumble"), instead of Mumble's (MainWindow::updateWindowTitle) */
	void updateWindowTitle();
	/** Mumble's own menus (Server, Self, Configure, Help, ...) as a popup at pos of the view, plus the user list */
	void showMumbleMenu(const QPoint &viewPos);
	/** Mumble's user tree as a window of its own: channel and user administration via its context menu */
	void setUserListVisible(bool visible);
	/**
	 * Mumble's own log and chat as docks. Shown by itself when the service's web UI has no Desk panel (an older
	 * Ruumble service), so there is always a chat; the Mumble menu switches it by hand.
	 */
	void setLogVisible(bool visible);
	/**
	 * Mumble's own context menus for a channel or a user of the building (right click in the web UI): every Mumble
	 * function (add, remove, edit, ACL, links, kick, ban, register, ...) with the user's own permissions. Destructive
	 * actions therefore always need a real click in a native menu, never a call of the page alone (ADR-0017).
	 * viewPos in CSS pixels of the page.
	 */
	void showChannelMenu(unsigned int channelId, const QPoint &viewPos);
	void showUserMenu(unsigned int session, const QPoint &viewPos);
	/** a person dragged onto a room: like a drag in Mumble's tree (follows its "user dragging" setting) */
	void moveUser(unsigned int session, unsigned int channelId);

	/** name of the plugin that the client must not load next to the built-in core */
	static QString externalPluginName() { return QStringLiteral("Ruumble"); }

	/**
	 * Before the settings are loaded: offers to take over an installed official Mumble (certificate, servers, audio,
	 * shortcuts, network), so every connection works as there (ADR-0024 D5, ticket #15; MumbleMigration).
	 */
	static void importMumbleSettings();
	/**
	 * Once, from importMumbleSettings: copies the settings and data of the former name "Ruumble Desk"
	 * (~/.config/Ruumble/Ruumble Desk, ~/.local/share/Ruumble/Ruumble Desk) into Ruumble Client's own, file by file
	 * without overwriting. Certificate, servers, pairing and the web view's cookies come along. Marker file
	 * ruumble-desk-taken-over next to the settings.
	 */
	static void takeOverDeskSettings();

signals:
	// from the core's threads to the view (queued)
	void serviceKnown(const QString &baseUrl);
	void linkRequested(const QString &url);
	void placeholderRequested(bool connected);

private:
	void watchTalking(unsigned int session);
	void onAction(const QString &action);
	void stop();

	MainWindow *m_mw;
	DeskChannel *m_desk;
	CityMap *m_cityMap;
	RuumbleView *m_view;
	QDockWidget *m_usersDock;
	QMenu *m_mumbleMenu = nullptr;
	bool m_userListVisible = false;
	bool m_logVisible = false;
	bool m_logChosen = false; // switched by hand in the Mumble menu: no automatic change any more
	bool m_panelSeen = false; // the current page asked for the log (Desk panel)
	std::unique_ptr< QtMumbleApi > m_api;
	std::unique_ptr< ruumble::WebSocketTransport > m_socket;
	std::unique_ptr< ObservingTransport > m_transport;
	std::unique_ptr< ruumble::Core > m_core;
};

} // namespace ruumbledesk

#endif
