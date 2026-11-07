// Ruumble Client: its own tray and window icons instead of Mumble's (tools/make-tray-icons.py writes them into
// client/tray). Mumble keeps them in public fields of its main window and only reads them from there, so replacing them
// right after the main window loaded its own (RuumbleIntegration's constructor, before main() creates the tray icon) is
// enough: Mumble itself is not changed. Header only: a new source file would need a line in Mumble's build list.

#ifndef MUMBLE_MUMBLE_RUUMBLE_TRAYICONS_H_
#define MUMBLE_MUMBLE_RUUMBLE_TRAYICONS_H_

#include "Global.h"
#include "Log.h"
#include "MainWindow.h"
#include "Settings.h"

#include <QtCore/QSettings>

#include <QtGui/QIcon>
#include <QtGui/QPainter>
#include <QtGui/QPixmap>
#include <QtSvg/QSvgRenderer>
#include <QtWidgets/QApplication>

namespace ruumbledesk {

/** one icon in the sizes a tray or a window list asks for: the small drawing (no screws, thicker lines) up to 24 px */
inline QIcon trayIcon(const QString &name) {
	QIcon icon;
	for (int px : { 16, 20, 22, 24, 32, 40, 48, 64, 96, 128, 256 }) {
		QSvgRenderer svg(QStringLiteral(":/ruumbledesk/tray/%1%2.svg").arg(name, px <= 24 ? QStringLiteral("-small") : QString()));
		if (!svg.isValid()) {
			continue;
		}
		QPixmap pixmap(px, px);
		pixmap.fill(Qt::transparent);
		QPainter painter(&pixmap);
		painter.setRenderHint(QPainter::Antialiasing);
		svg.render(&painter);
		painter.end();
		icon.addPixmap(pixmap);
	}
	return icon;
}

/** replaces Mumble's icons of the window and of every tray state */
inline void applyTrayIcons(MainWindow *mw) {
	mw->qiIcon               = trayIcon(QStringLiteral("app"));
	mw->qiTalkingOff         = trayIcon(QStringLiteral("talking-off"));
	mw->qiTalkingOn          = trayIcon(QStringLiteral("talking-on"));
	mw->qiTalkingWhisper     = trayIcon(QStringLiteral("talking-whisper"));
	mw->qiTalkingShout       = trayIcon(QStringLiteral("talking-shout"));
	mw->qiIconMuteSelf       = trayIcon(QStringLiteral("muted-self"));
	mw->qiIconMuteServer     = trayIcon(QStringLiteral("muted-server"));
	mw->qiIconMuteSuppressed = trayIcon(QStringLiteral("muted-suppressed"));
	mw->qiIconMutePushToMute = trayIcon(QStringLiteral("muted-push-to-mute"));
	mw->qiIconDeafSelf       = trayIcon(QStringLiteral("deafened-self"));
	mw->qiIconDeafServer     = trayIcon(QStringLiteral("deafened-server"));
	mw->m_iconInformation    = trayIcon(QStringLiteral("information"));
	// Mumble set its own as the window icon: the window list showed it next to Ruumble's taskbar entry
	qApp->setWindowIcon(mw->qiIcon);
	mw->setWindowIcon(mw->qiIcon);
}

/**
 * Mumble's pop-up notifications ("balloons") for events: it shows them whenever its log is not visible, and in Ruumble
 * Client the log is never visible (the building and the Desk panel are), so every mute, room change or listening came
 * as a desktop notification. Each one also puts the tray icon into "needs attention" with Qt's information icon, so
 * the tray showed a blue "i" instead of the state. Once, they go off for everything the building shows anyway; text
 * messages, warnings and errors keep them. The user's later choice in Mumble's settings stays.
 */
inline void quietBalloons() {
	QSettings marker;
	const QString key = QStringLiteral("tray/balloonsQuieted");
	if (marker.value(key, false).toBool()) {
		return;
	}
	marker.setValue(key, true);
	Settings &s = Global::get().s;
	for (int type = Log::firstMsgType; type <= Log::lastMsgType; ++type) {
		if (type == Log::TextMessage || type == Log::PrivateTextMessage || type == Log::Warning
			|| type == Log::CriticalError || type == Log::YouKicked) {
			continue;
		}
		s.qmMessages[type] = s.qmMessages.value(type) & ~static_cast< quint32 >(Settings::LogBalloon);
	}
	s.save();
}

} // namespace ruumbledesk

#endif
