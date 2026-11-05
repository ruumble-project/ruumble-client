// Ruumble Client: the Ruumble City Map, Mumble's connection dialog in the building look (ticket #8).
//
// The page (citymap/citymap.html, bundled) talks to this object over QWebChannel as "cityMap". Every call checks that
// the page asking is the bundled City Map, never the Ruumble service or another page; signals only go out while the
// City Map is shown, so a service page never learns the saved addresses. The access code goes into the page never:
// the page can only set a new one.
//
// Addresses are Mumble's favourites (Database), so Mumble's own list and the City Map show the same. "On your next
// visit" is Mumble's "connect automatically to the last server" (Settings::bAutoConnect) plus, for headquarters, the
// chosen address in Ruumble Client's own file city-map.json (Mumble rewrites its favourites table on every save, an
// extra column there would be lost).

#ifndef MUMBLE_MUMBLE_RUUMBLE_CITYMAP_H_
#define MUMBLE_MUMBLE_RUUMBLE_CITYMAP_H_

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QVariant>

#include <functional>

class MainWindow;
class QTimer;
struct FavoriteServer;

namespace ruumbledesk {

class ServerPing;

class CityMap : public QObject {
	Q_OBJECT

public:
	/**
	 * allowed: the page asking is the City Map. show / leave: put the City Map into the view, or go back to what was
	 * there before (the building or the start card).
	 */
	CityMap(MainWindow *mw, std::function< bool() > allowed, std::function< void() > show,
			std::function< void() > leave, QObject *parent);
	~CityMap() override;

	/**
	 * Instead of Mumble's connection dialog (MainWindow::openServerConnectDialog). autoconnect: the call at start;
	 * then the building of "On your next visit" is entered, once per run. Returns false only after bypassOnce().
	 */
	bool open(bool autoconnect);
	/** the next open() leaves it to Mumble's own server list (Mumble menu) */
	void bypassOnce() { m_bypass = true; }
	/**
	 * From MainWindow::serverDisconnected (not for the certificate question): while a building is being entered from
	 * the City Map, the failure is shown there and Mumble shows no dialog and does not reconnect. Returns whether it was
	 * taken over.
	 */
	bool connectionFailed(int socketError, const QString &reason, int rejectType);
	/** the City Map or an entering is on screen: the start card must not replace it */
	bool onScreen() const { return m_shown || m_entry.active; }
	/** the view now shows something else */
	void pageLeft();
	/** the building is there */
	void entered();

	// the page
	Q_INVOKABLE QVariantMap state();
	/** id -1: new address. Returns "" or the field that is wrong: "name", "host", "port", "user" */
	Q_INVOKABLE QString save(int id, const QVariantMap &entry);
	Q_INVOKABLE void remove(int id);
	/** mode "never", "last" or "hq" (with the address id) */
	Q_INVOKABLE void setNextVisit(const QString &mode, int id);
	Q_INVOKABLE void enter(int id);
	Q_INVOKABLE void tryAgain();
	/** Cancel: of the City Map, or of an entering */
	Q_INVOKABLE void cancel();
	/** the page is loaded and listens */
	Q_INVOKABLE void ready();

signals:
	/** addresses, choice, note or entering changed: the page asks for state() again */
	void changed();
	/** new connection and visitor figures: [{id, ms, users, maxUsers, answered, gaveUp}] */
	void pinged(const QVariantList &results);

private:
	struct Entry {
		bool active    = false;
		bool automatic = false;
		bool timedOut  = false;
		bool started   = false; // the server handler ran at least once
		QString name, host, user;
		unsigned short port = 0;
		int elapsedMs       = 0;
	};
	struct Note {
		QString building; // empty: no note
		QString reason;   // "timeout", "refused", "nameInUse", "tls", "unreachable", "notFound"
		QString detail;
	};

	QList< FavoriteServer > favorites() const;
	void store(const QList< FavoriteServer > &list);
	QString mode() const;
	int headquarters(const QList< FavoriteServer > &list) const;
	void setHeadquarters(const FavoriteServer *fs);
	void loadOwnSettings();
	void saveOwnSettings() const;
	void startEntry(const FavoriteServer &fs, bool automatic);
	void tickEntry();
	void fail(const QString &reason, const QString &detail = QString());
	void showMap();
	void updatePing();
	void sendPing();
	void notify();

	MainWindow *m_mw;
	std::function< bool() > m_allowed;
	std::function< void() > m_show;
	std::function< void() > m_leave;
	ServerPing *m_ping;
	QTimer *m_entryTimer;
	bool m_shown         = false;
	bool m_bypass        = false;
	bool m_autoTried     = false;
	int m_stoppedTicks   = 0;
	bool m_swallowNext   = false; // the disconnect after Cancel: no note, no Mumble dialog, no reconnect
	bool m_hq            = false; // own setting: "On your next visit" is headquarters
	QString m_hqHost, m_hqUser;
	unsigned short m_hqPort = 0;
	Entry m_entry;
	Note m_note;
	QString m_lastTried; // name of the building of the last entering, for "Try again"
};

} // namespace ruumbledesk

#endif
