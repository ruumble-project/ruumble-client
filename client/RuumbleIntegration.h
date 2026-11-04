// Ruumble Desk: Ruumble built into the client (ADR-0024, phases 0 and 1).
// Runs Ruumble's core like the plugin does and shows the web UI as the main view.
// The only entry points from Mumble code are the constructor (MainWindow::setupGui) and
// attachServerHandler (MainWindow, where a ServerHandler is created).

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

	/** name of the plugin that the client must not load next to the built-in core */
	static QString externalPluginName() { return QStringLiteral("Ruumble"); }

	/**
	 * Once, before the settings are loaded: takes the audio settings and the shortcuts (push-to-talk and others) of
	 * an installed official Mumble into Ruumble Desk's own settings, which are kept apart otherwise (ADR-0024 D5).
	 * Not the mute and deaf state of its last session. A marker file next to the settings makes it run only once.
	 */
	static void importMumbleSettings();

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
