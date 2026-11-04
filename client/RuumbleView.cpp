#include "RuumbleView.h"

#include "Global.h"

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QStandardPaths>
#include <QtGui/QDesktopServices>
#include <QtWebChannel/QWebChannel>
#include <QtWebEngineCore/QWebEngineDownloadRequest>
#include <QtWebEngineCore/QWebEnginePage>
#include <QtWebEngineCore/QWebEngineProfile>
#include <QtWebEngineCore/QWebEngineScript>
#include <QtWebEngineCore/QWebEngineScriptCollection>

namespace ruumbledesk {

namespace {

	const QString kActionScheme = QStringLiteral("ruumbledesk");

	bool sameOrigin(const QUrl &a, const QUrl &b) {
		return a.scheme() == b.scheme() && a.host() == b.host() && a.port() == b.port();
	}

	/**
	 * Page of the main view. Links that leave the service and pages that want a new window
	 * (target=_blank) go to the system browser, so the view always stays on the building.
	 * ruumbledesk:<action> links (placeholder page only) trigger client actions.
	 */
	class Page : public QWebEnginePage {
	public:
		Page(QWebEngineProfile *profile, const QUrl *service, std::function< void(const QString &) > onAction,
			 QObject *parent)
			: QWebEnginePage(profile, parent), m_service(service), m_onAction(std::move(onAction)) {}

	protected:
		bool acceptNavigationRequest(const QUrl &url, NavigationType type, bool isMainFrame) override {
			if (url.scheme() == kActionScheme) {
				// only from the own placeholder page, never from the service or a foreign page
				if (type == NavigationTypeLinkClicked && !m_service->isValid()) {
					m_onAction(url.path());
				}
				return false;
			}
			if (isMainFrame && type == NavigationTypeLinkClicked && m_service->isValid()
				&& !sameOrigin(url, *m_service)) {
				QDesktopServices::openUrl(url);
				return false;
			}
			return QWebEnginePage::acceptNavigationRequest(url, type, isMainFrame);
		}

		QWebEnginePage *createWindow(WebWindowType) override {
			// a throwaway page: the first URL it is asked to load goes to the system browser
			auto *popup = new QWebEnginePage(profile(), this);
			QObject::connect(popup, &QWebEnginePage::urlChanged, popup, [popup](const QUrl &url) {
				if (url.isValid() && !url.isEmpty()) {
					QDesktopServices::openUrl(url);
				}
				popup->deleteLater();
			});
			return popup;
		}

	private:
		const QUrl *m_service;
		std::function< void(const QString &) > m_onAction;
	};

	/** Qt's qwebchannel.js, injected so the Ruumble UI can find the desk object (web/src/lib/desk/) */
	QWebEngineScript webChannelScript() {
		QFile file(QStringLiteral(":/qtwebchannel/qwebchannel.js"));
		QWebEngineScript script;
		if (file.open(QIODevice::ReadOnly)) {
			script.setSourceCode(QString::fromUtf8(file.readAll()));
		}
		script.setName(QStringLiteral("ruumbledesk-webchannel"));
		script.setInjectionPoint(QWebEngineScript::DocumentCreation);
		script.setWorldId(QWebEngineScript::MainWorld);
		script.setRunsOnSubFrames(false);
		return script;
	}

	/**
	 * Qt WebEngine 6.4 is Chromium 102: no popover API, no color-mix(), no container units. A web UI from this repo
	 * copes with that itself, one served by an older Ruumble service does not (room descriptions always shown, person
	 * cards misplaced, dark board). This script fills the gaps only where the engine lacks them (compat/README.md).
	 */
	QWebEngineScript compatScript() {
		QString polyfill;
		QFile file(QStringLiteral(":/ruumbledesk/popover-polyfill.js"));
		if (file.open(QIODevice::ReadOnly)) {
			polyfill = QString::fromUtf8(file.readAll());
		}
		const QString source =
			QStringLiteral(
				"(function () {\n"
				"  if (!('showPopover' in HTMLElement.prototype)) {\n%1\n}\n"
				"  if (window.CSS && CSS.supports('color', 'color-mix(in srgb, red, blue)')) return;\n"
				// declarations with var() in them are only dropped when computed: the fallbacks must win the cascade
				// (specificity above the UI's scoped .board.svelte-x rules, appended after its stylesheet)
				"  var css = 'html:root{--color-board:#e9f3fc}'\n"
				"    + '@supports not (width:1cqw){.board.board.board{width:var(--board-width)}}'\n"
				"    + '.board .card.card:hover,.board .card.card:focus-within,.board .card.card.active'\n"
				"    + '{background:var(--color-blue-50)}';\n"
				"  function add() {\n"
				"    var style = document.createElement('style');\n"
				"    style.id = 'ruumbledesk-compat';\n"
				"    style.textContent = css;\n"
				"    (document.head || document.documentElement).appendChild(style);\n"
				"  }\n"
				"  if (document.readyState === 'loading') document.addEventListener('DOMContentLoaded', add); else add();\n"
				"})();")
				.arg(polyfill);
		QWebEngineScript script;
		script.setSourceCode(source);
		script.setName(QStringLiteral("ruumbledesk-compat"));
		script.setInjectionPoint(QWebEngineScript::DocumentCreation);
		script.setWorldId(QWebEngineScript::MainWorld);
		script.setRunsOnSubFrames(false);
		return script;
	}

	/** placeholder in Ruumble's look (colors of web/src/app.css) */
	QString placeholderHtml(bool connected) {
		const QString text = connected
								 ? RuumbleView::tr("This Mumble server does not offer Ruumble yet. Its top channel "
												   "needs a line \"ruumble: &lt;address&gt;\" in its description.")
								 : RuumbleView::tr("Connect to a Mumble server that offers Ruumble. The building "
												   "appears here.");
		return QStringLiteral(
				   "<!doctype html><html><head><meta charset=\"utf-8\"><style>"
				   "html,body{margin:0;height:100%;font-family:Inter,'Segoe UI',system-ui,sans-serif;"
				   "background:#003869;color:#003869}"
				   "main{height:100%;display:flex;align-items:center;justify-content:center;padding:24px;"
				   "box-sizing:border-box}"
				   ".card{max-width:460px;background:#fff;border-radius:10px;padding:32px 36px;text-align:center;"
				   "box-shadow:0 16px 48px rgb(0 0 0/.3)}"
				   ".sign{display:inline-block;padding:6px 18px;border-radius:6px;background:#003869;color:#fff;"
				   "font-weight:800;font-size:22px;letter-spacing:.02em;margin-bottom:18px}"
				   "p{font-size:15px;line-height:1.5;color:#00508c;margin:0 0 22px}"
				   ".row{display:flex;gap:10px;justify-content:center;flex-wrap:wrap}"
				   "a{display:inline-block;min-height:40px;line-height:40px;padding:0 18px;border-radius:6px;"
				   "border:1px solid #9ccaf1;color:#003869;text-decoration:none;font-weight:700;font-size:14px}"
				   "a.solid{background:#003869;border-color:#003869;color:#fff}a:hover{filter:brightness(1.08)}"
				   "</style></head><body><main><div class=\"card\"><div class=\"sign\">Ruumble</div>"
				   "<p>%1</p><div class=\"row\">"
				   "<a class=\"solid\" href=\"ruumbledesk:connect\">%2</a>"
				   "<a href=\"ruumbledesk:settings\">%3</a>"
				   "<a href=\"ruumbledesk:audio\">%4</a>"
				   "</div></div></main></body></html>")
			.arg(text, RuumbleView::tr("Connect to a server"), RuumbleView::tr("Settings"),
				 RuumbleView::tr("Audio wizard"));
	}

} // namespace

RuumbleView::RuumbleView(QObject *desk, std::function< void(const QString &) > onAction, QWidget *parent)
	: QWebEngineView(parent) {
	// Named profile with storage under the client's own data dir: the pairing cookie
	// (Max-Age one year) and the UI's localStorage survive a restart.
	m_profile = new QWebEngineProfile(QLatin1String("RuumbleDesk"), this);
	const QString base = Global::get().qdBasePath.filePath(QLatin1String("ruumble-web"));
	m_profile->setPersistentStoragePath(base);
	m_profile->setCachePath(QDir(base).filePath(QLatin1String("cache")));
	m_profile->setPersistentCookiesPolicy(QWebEngineProfile::AllowPersistentCookies);
	m_profile->scripts()->insert(webChannelScript());
	m_profile->scripts()->insert(compatScript());

	// downloads (board files, tube mail, care export) go to the user's download folder
	QObject::connect(m_profile, &QWebEngineProfile::downloadRequested, this, [](QWebEngineDownloadRequest *download) {
		const QString dir = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
		if (!dir.isEmpty()) {
			download->setDownloadDirectory(dir);
		}
		download->accept();
	});

	auto *page    = new Page(m_profile, &m_service, std::move(onAction), this);
	auto *channel = new QWebChannel(page);
	channel->registerObject(QStringLiteral("desk"), desk);
	page->setWebChannel(channel);
	setPage(page);
	setContextMenuPolicy(Qt::NoContextMenu);
	showPlaceholder(false);
}

RuumbleView::~RuumbleView() {
	// the page must go before its profile (both are children of the view, the profile was created first)
	delete page();
}

bool RuumbleView::showsService() const {
	return m_service.isValid() && sameOrigin(url(), m_service);
}

void RuumbleView::showService(const QString &baseUrl) {
	const QUrl service(baseUrl);
	if (!service.isValid()) {
		return;
	}
	m_service = service;
	if (!sameOrigin(url(), service)) {
		load(service);
	}
}

void RuumbleView::openLink(const QString &link) {
	const QUrl target(link);
	if (target.scheme() != QLatin1String("http") && target.scheme() != QLatin1String("https")) {
		return;
	}
	if (m_service.isValid() && !sameOrigin(target, m_service)) {
		QDesktopServices::openUrl(target);
		return;
	}
	load(target);
}

void RuumbleView::showPlaceholder(bool connected) {
	m_service = QUrl();
	setHtml(placeholderHtml(connected));
}

} // namespace ruumbledesk
