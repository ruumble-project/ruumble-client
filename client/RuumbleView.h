// Ruumble Desk: the Ruumble web UI as the client's whole window (ADR-0024, phase 1).

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
	/** onAction gets the actions of the placeholder page: "connect", "settings", "audio" */
	RuumbleView(QObject *desk, std::function< void(const QString &) > onAction, QWidget *parent = nullptr);
	~RuumbleView() override;

	/** the page shown is the Ruumble service (and so may use the desk object) */
	bool showsService() const;

public slots:
	/** the core connects to this service: show its UI unless it is shown already */
	void showService(const QString &baseUrl);
	/** a link the plugin would open in the browser (the pairing link): open it here */
	void openLink(const QString &url);
	/** no server or no service: placeholder with the ways to get connected */
	void showPlaceholder(bool connected);

private:
	QWebEngineProfile *m_profile;
	QUrl m_service;
};

} // namespace ruumbledesk

#endif
