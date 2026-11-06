#include "TransmitControl.h"

#include "PttKeys.h"

#include "AudioInput.h"
#include "ClientUser.h"
#include "Global.h"
#include "GlobalShortcut.h"
#include "MainWindow.h"

#include <QtGui/QGuiApplication>

#include <algorithm>
#include <cmath>

namespace ruumbledesk {

namespace {

	/** how often the talk state and the level are read: fast enough to feel instant, cheap enough to not matter */
	constexpr int kPollMs = 50;
	/** mode, key and options are compared every this many polls (Mumble's settings dialog changes them silently) */
	constexpr int kSettingsEvery = 10;
	/** the level meter's range: speech sits between about -40 and -10 dB, so -60 dB is empty and 0 dB full */
	constexpr float kQuietDb = -60.0f;
	/** a double press keeps the mic open when Ruumble turns it on and Mumble has no time set (microseconds) */
	constexpr quint64 kDoublePushUs = 300000;

	// buttons a capture never takes: the left and right mouse button and the wheel (they click the page itself), and
	// Escape (it cancels)
	bool capturable(const QVariant &button) {
		bool ok              = false;
		const unsigned int b = button.toUInt(&ok);
		if (!ok) {
			return true; // another engine's format: take it as it is
		}
		if (b == 9) { // Escape (evdev 1)
			return false;
		}
		const unsigned int mouse = b - pttkeys::kMouseBase;
		return !(b > pttkeys::kMouseBase && (mouse == 1 || mouse == 3 || (mouse >= 4 && mouse <= 7)));
	}

	QString modeName(Settings::AudioTransmit mode) {
		switch (mode) {
			case Settings::PushToTalk:
				return QStringLiteral("ptt");
			case Settings::Continuous:
				return QStringLiteral("continuous");
			default:
				return QStringLiteral("vad");
		}
	}

} // namespace

TransmitControl::TransmitControl(MainWindow *mw, QObject *parent) : QObject(parent), m_mw(mw) {
	m_lastMode    = static_cast< int >(Global::get().s.atTransmit);
	m_lastButtons = boundButtons();
	m_lastOptions = options();
	// Mumble's own switches (shortcut, D-Bus, its menu) report at once; its settings dialog is seen by poll()
	QObject::connect(mw, &MainWindow::transmissionModeChanged, this, [this] { poll(); });
	QObject::connect(&m_timer, &QTimer::timeout, this, &TransmitControl::poll);
	m_timer.start(kPollMs);
}

TransmitControl::~TransmitControl() {
	releasePage();
}

QString TransmitControl::mode() const {
	return modeName(Global::get().s.atTransmit);
}

bool TransmitControl::setMode(const QString &mode) {
	Settings::AudioTransmit next;
	if (mode == QLatin1String("ptt")) {
		next = Settings::PushToTalk;
	} else if (mode == QLatin1String("vad")) {
		next = Settings::VAD;
	} else if (mode == QLatin1String("continuous")) {
		next = Settings::Continuous;
	} else {
		return false;
	}
	m_mw->setTransmissionMode(next); // logs the change and emits transmissionModeChanged, which polls
	Global::get().s.save();
	return true;
}

void TransmitControl::trigger(bool down) {
	// Mumble counts presses (Global::iPushToTalk): the page's presses add to the key's, never replace them
	m_mw->on_PushToTalk_triggered(down, QVariant());
}

void TransmitControl::press(bool down) {
	if (down == m_pressed || (down && Global::get().s.atTransmit != Settings::PushToTalk)) {
		return;
	}
	m_pressed = down;
	trigger(down);
}

void TransmitControl::setLatched(bool on) {
	if (on == m_latched || (on && Global::get().s.atTransmit != Settings::PushToTalk)) {
		return;
	}
	m_latched = on;
	trigger(on);
	emit latchedChanged();
}

void TransmitControl::releasePage() {
	press(false);
	setLatched(false);
}

void TransmitControl::watchLevel(bool on) {
	m_watch = on;
}

void TransmitControl::poll() {
	const Settings &s = Global::get().s;

	const ClientUser *self = Global::get().uiSession ? ClientUser::get(Global::get().uiSession) : nullptr;
	const bool transmitting =
		self && self->tsState != Settings::Passive && self->tsState != Settings::MutedTalking;
	if (transmitting != m_transmitting) {
		m_transmitting = transmitting;
		emit transmittingChanged();
	}

	double level = 0;
	if (m_watch || m_transmitting) {
		const AudioInputPtr ai = Global::get().ai;
		// exactly 0 dB is Mumble's start value before the first frame (a real signal practically never hits it): no
		// input yet must not look like a full meter
		if (ai && ai->dPeakCleanMic < 0.0f) {
			const float db = std::max(ai->dPeakCleanMic, kQuietDb);
			level          = std::round((db - kQuietDb) / -kQuietDb * 50.0) / 50.0; // steps of 0.02: fewer updates
		}
	}
	if (level != m_level) {
		m_level = level;
		emit levelChanged();
	}

	if (++m_tick < kSettingsEvery && static_cast< int >(s.atTransmit) == m_lastMode) {
		return;
	}
	m_tick = 0;
	if (static_cast< int >(s.atTransmit) != m_lastMode) {
		m_lastMode = static_cast< int >(s.atTransmit);
		if (s.atTransmit != Settings::PushToTalk) {
			releasePage();
		}
		emit modeChanged();
	}
	const QList< QVariant > buttons = boundButtons();
	if (buttons != m_lastButtons) {
		m_lastButtons = buttons;
		emit keyChanged();
	}
	const QVariantMap opts = options();
	if (opts != m_lastOptions) {
		m_lastOptions = opts;
		emit optionsChanged();
	}
}

QList< QVariant > TransmitControl::boundButtons() const {
	// the first push-to-talk entry with buttons; Mumble allows several, Ruumble shows and sets one
	for (const Shortcut &sc : Global::get().s.qlShortcuts) {
		if (sc.iIndex == m_mw->gsPushTalk->idx && !sc.qlButtons.isEmpty()) {
			return sc.qlButtons;
		}
	}
	return {};
}

QString TransmitControl::scope() const {
	// Mumble's engine on Linux is X11 (XInput2): it hears every window's keys only when the client itself runs on X11;
	// under Wayland (and offscreen) it hears nothing outside, the page then listens while the window has the focus
	if (QGuiApplication::platformName() != QLatin1String("xcb") || !GlobalShortcutEngine::engine
		|| !Global::get().s.bShortcutEnable) {
		return QStringLiteral("window");
	}
	return QStringLiteral("global");
}

QVariantMap TransmitControl::describe(const QList< QVariant > &buttons) const {
	// label: Mumble's names ("Control_R"), the page translates the codes it knows; codes only when all are known
	QStringList names;
	QStringList codes;
	bool known = true;
	bool warn  = false;
	// Mumble's X11 engine crashes when asked for a name without a display connection (Wayland without XWayland,
	// offscreen): it is only asked on X11
	const bool x11 = QGuiApplication::platformName() == QLatin1String("xcb") && GlobalShortcutEngine::engine;
	for (const QVariant &button : buttons) {
		const QString code = pttkeys::toCode(button.toUInt());
		names << (x11 ? GlobalShortcutEngine::engine->buttonInfo(button).name
					  : (code.isEmpty() ? QStringLiteral("0x%1").arg(button.toUInt(), 0, 16) : code));
		if (code.isEmpty()) {
			known = false;
		} else {
			codes << code;
			warn = warn || (buttons.size() == 1 && pttkeys::typesElsewhere(code));
		}
	}
	return { { QStringLiteral("set"), !buttons.isEmpty() },
			 { QStringLiteral("label"), names.join(QStringLiteral(" + ")) },
			 { QStringLiteral("codes"), known ? codes : QStringList() },
			 { QStringLiteral("scope"), scope() },
			 { QStringLiteral("warn"), warn } };
}

QVariantMap TransmitControl::key() const {
	return describe(boundButtons());
}

QVariantMap TransmitControl::options() const {
	const Settings &s = Global::get().s;
	return { { QStringLiteral("doublePush"), s.uiDoublePush > 0 },
			 { QStringLiteral("audioCue"), s.audioCueEnabledPTT } };
}

bool TransmitControl::setOption(const QString &name, const QVariant &value) {
	Settings &s   = Global::get().s;
	const bool on = value.toBool();
	if (name == QLatin1String("doublePush")) {
		if (on != (s.uiDoublePush > 0)) {
			s.uiDoublePush = on ? kDoublePushUs : 0;
		}
	} else if (name == QLatin1String("audioCue")) {
		s.audioCueEnabledPTT = on;
	} else {
		return false;
	}
	s.save();
	const QVariantMap opts = options();
	if (opts != m_lastOptions) {
		m_lastOptions = opts;
		emit optionsChanged();
	}
	return true;
}

bool TransmitControl::startCapture() {
	// the last captured key stays pending until a new one is captured: the page listens again after each key, so the
	// user can try another one and still accept the one shown
	stopCapture();
	m_capturing = true;
	if (GlobalShortcutEngine *engine = GlobalShortcutEngine::engine) {
		engine->resetMap();
		// as Mumble's own button dialog: every button pressed until the first release
		m_captureConnection = QObject::connect(engine, &GlobalShortcutEngine::buttonPressed, this, [this](bool last) {
			if (last && m_capturing && GlobalShortcutEngine::engine) {
				finishCapture(GlobalShortcutEngine::engine->qlActiveButtons);
			}
		});
	}
	return true;
}

void TransmitControl::capturePageKey(const QStringList &codes) {
	if (!m_capturing || codes.isEmpty()) {
		return;
	}
	QList< QVariant > buttons;
	for (const QString &code : codes) {
		const unsigned int button = pttkeys::toMumble(code);
		if (button == 0) {
			return; // a key the table does not know: only Mumble's engine can bind it
		}
		buttons << QVariant(button);
	}
	finishCapture(buttons);
}

void TransmitControl::finishCapture(QList< QVariant > buttons) {
	buttons.erase(std::remove_if(buttons.begin(), buttons.end(), [](const QVariant &b) { return !capturable(b); }),
				  buttons.end());
	if (buttons.isEmpty()) {
		if (GlobalShortcutEngine::engine) {
			GlobalShortcutEngine::engine->resetMap(); // a click on the page or Escape: keep listening
		}
		return;
	}
	stopCapture();
	m_pending = buttons;
	emit keyCaptured(describe(buttons));
}

void TransmitControl::stopCapture() {
	m_capturing = false;
	QObject::disconnect(m_captureConnection);
}

void TransmitControl::cancelCapture() {
	stopCapture();
	m_pending.clear();
}

bool TransmitControl::acceptKey() {
	if (m_pending.isEmpty()) {
		return false;
	}
	Settings &s     = Global::get().s;
	const int index = m_mw->gsPushTalk->idx;
	s.qlShortcuts.erase(std::remove_if(s.qlShortcuts.begin(), s.qlShortcuts.end(),
									   [index](const Shortcut &sc) { return sc.iIndex == index; }),
						s.qlShortcuts.end());
	Shortcut sc;
	sc.iIndex    = index;
	sc.qlButtons = m_pending;
	s.qlShortcuts << sc;
	m_pending.clear();
	// a key set here is meant to work: global shortcuts on, as Mumble's settings dialog would switch them
	s.bShortcutEnable = true;
	if (GlobalShortcutEngine *engine = GlobalShortcutEngine::engine) {
		engine->bNeedRemap = true;
		engine->needRemap();
		engine->setEnabled(true);
	}
	s.save();
	m_lastButtons = boundButtons();
	emit keyChanged();
	return true;
}

} // namespace ruumbledesk
