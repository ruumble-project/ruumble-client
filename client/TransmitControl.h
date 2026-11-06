// Ruumble Client: how the user transmits, for the Ruumble web UI (ticket #9). Mumble's own transmit mode, its
// push-to-talk shortcut and its press counter are the single source of truth: this class only reads and changes them,
// so Mumble's settings dialog and the page always show the same.
//
// Presses from the page are held here (one momentary press, one latch) and released by the client itself when the
// page goes away, the mode changes or the client closes: a press from the page can never stay down.

#ifndef MUMBLE_MUMBLE_RUUMBLE_TRANSMITCONTROL_H_
#define MUMBLE_MUMBLE_RUUMBLE_TRANSMITCONTROL_H_

#include <QtCore/QList>
#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QTimer>
#include <QtCore/QVariant>

class MainWindow;

namespace ruumbledesk {

class TransmitControl : public QObject {
	Q_OBJECT

public:
	TransmitControl(MainWindow *mw, QObject *parent);
	~TransmitControl() override;

	/** "vad", "ptt" or "continuous" */
	QString mode() const;
	bool setMode(const QString &mode);
	/** the own user is transmitting right now (Mumble's talk state, whatever started it) */
	bool transmitting() const { return m_transmitting; }
	/** the microphone level, 0..1; only kept up to date while watched or transmitting */
	double level() const { return m_level; }
	bool latched() const { return m_latched; }
	/** { set, label, codes, scope, warn }: the bound push-to-talk key (see describe) */
	QVariantMap key() const;
	/** { doublePush, audioCue } */
	QVariantMap options() const;

	/** the page's momentary press (mouse on the microphone or the key while the window has the focus) */
	void press(bool down);
	/** the page's latch: transmits until released */
	void setLatched(bool on);
	void watchLevel(bool on);
	/** listens for the next key or mouse button through Mumble's shortcut engine (and the page, see capturePageKey) */
	bool startCapture();
	/** a key the page saw while capturing (DOM codes); the only source on Wayland, where the engine sees nothing */
	void capturePageKey(const QStringList &codes);
	void cancelCapture();
	/** binds the captured key as Mumble's push-to-talk shortcut (the same entry Mumble's settings use) */
	bool acceptKey();
	bool setOption(const QString &name, const QVariant &value);
	/** lets go of everything the page holds */
	void releasePage();

signals:
	void modeChanged();
	void transmittingChanged();
	void levelChanged();
	void latchedChanged();
	void keyChanged();
	void optionsChanged();
	void keyCaptured(const QVariantMap &key);

private:
	void poll();
	void trigger(bool down);
	void finishCapture(QList< QVariant > buttons);
	void stopCapture();
	QList< QVariant > boundButtons() const;
	QVariantMap describe(const QList< QVariant > &buttons) const;
	QString scope() const;

	MainWindow *m_mw;
	QTimer m_timer;
	int m_tick = 0;
	bool m_pressed = false;
	bool m_latched = false;
	bool m_transmitting = false;
	double m_level = 0;
	bool m_watch = false;
	int m_lastMode = -1;
	QList< QVariant > m_lastButtons;
	QVariantMap m_lastOptions;
	bool m_capturing = false;
	QList< QVariant > m_pending;
	QMetaObject::Connection m_captureConnection;
};

} // namespace ruumbledesk

#endif
