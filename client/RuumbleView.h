// Ruumble Client: the Ruumble web UI as the client's whole window (ADR-0024, phase 1).

#ifndef MUMBLE_MUMBLE_RUUMBLE_RUUMBLEVIEW_H_
#define MUMBLE_MUMBLE_RUUMBLE_RUUMBLEVIEW_H_

#include <QtCore/QUrl>
#include <QtWebEngineWidgets/QWebEngineView>

#include <functional>

class QWebEngineProfile;

namespace ruumbledesk {

class RuumbleView : public QWebEngineView {
	Q_OBJECT

public:
	/**
	 * desk: object "desk" for the service's page. cityMap: object "cityMap" for the bundled City Map page.
	 * onAction gets the actions of the placeholder page: "connect", "settings", "audio".
	 */
	RuumbleView(QObject *desk, QObject *cityMap, std::function< void(const QString &) > onAction,
				QWidget *parent = nullptr);
	~RuumbleView() override;

	/** the page shown is the Ruumble service (and so may use the desk object) */
	bool showsService() const;
	/** the page shown is the bundled City Map (and so may use the cityMap object) */
	bool showsCityMap() const;
	static bool isCityMap(const QUrl &url);

public slots:
	/** the core connects to this service: show its UI unless it is shown already */
	void showService(const QString &baseUrl);
	/** a link the plugin would open in the browser (the pairing link): open it here */
	void openLink(const QString &url);
	/** no server or no service: placeholder with the ways to get connected */
	void showPlaceholder(bool connected);
	/** the City Map instead of Mumble's connection dialog (ticket #8) */
	void showCityMap();
	/** back from the City Map: the building of the service if there is one, else the placeholder */
	void leaveCityMap(bool connected);
	/** the connection to the service's server is gone, but the page stays (the City Map is on screen) */
	void forgetService();

private:
	QWebEngineProfile *m_profile;
	QUrl m_service;
};

} // namespace ruumbledesk

#endif
