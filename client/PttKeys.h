// Ruumble Client: the push-to-talk key between the page and Mumble's global shortcuts (ticket #9).
// The page names keys by their DOM code ("ControlRight"), Mumble's X11 engine by the X key code (Linux evdev code + 8)
// and mouse buttons by the X button number + 0x117. This table maps the keys a person would bind to talk; anything
// else is still bound through Mumble's own capture, it just has no DOM code (the page then cannot listen for it on
// Wayland, where it only sees keys while the window has the focus).

#ifndef MUMBLE_MUMBLE_RUUMBLE_PTTKEYS_H_
#define MUMBLE_MUMBLE_RUUMBLE_PTTKEYS_H_

#include <QtCore/QString>

#include <utility>

namespace ruumbledesk {
namespace pttkeys {

	/** offset of Mumble's mouse buttons on X11 (GlobalShortcut_unix.cpp: detail + 0x117) */
	constexpr unsigned int kMouseBase = 0x117;
	/** X key codes are Linux evdev codes plus 8 */
	constexpr unsigned int kEvdevToX = 8;

	// DOM code, Linux evdev code
	inline constexpr std::pair< const char *, unsigned int > kKeys[] = {
		{ "Escape", 1 },        { "Digit1", 2 },          { "Digit2", 3 },          { "Digit3", 4 },
		{ "Digit4", 5 },        { "Digit5", 6 },          { "Digit6", 7 },          { "Digit7", 8 },
		{ "Digit8", 9 },        { "Digit9", 10 },         { "Digit0", 11 },         { "Minus", 12 },
		{ "Equal", 13 },        { "Backspace", 14 },      { "Tab", 15 },            { "KeyQ", 16 },
		{ "KeyW", 17 },         { "KeyE", 18 },           { "KeyR", 19 },           { "KeyT", 20 },
		{ "KeyY", 21 },         { "KeyU", 22 },           { "KeyI", 23 },           { "KeyO", 24 },
		{ "KeyP", 25 },         { "BracketLeft", 26 },    { "BracketRight", 27 },   { "Enter", 28 },
		{ "ControlLeft", 29 },  { "KeyA", 30 },           { "KeyS", 31 },           { "KeyD", 32 },
		{ "KeyF", 33 },         { "KeyG", 34 },           { "KeyH", 35 },           { "KeyJ", 36 },
		{ "KeyK", 37 },         { "KeyL", 38 },           { "Semicolon", 39 },      { "Quote", 40 },
		{ "Backquote", 41 },    { "ShiftLeft", 42 },      { "Backslash", 43 },      { "KeyZ", 44 },
		{ "KeyX", 45 },         { "KeyC", 46 },           { "KeyV", 47 },           { "KeyB", 48 },
		{ "KeyN", 49 },         { "KeyM", 50 },           { "Comma", 51 },          { "Period", 52 },
		{ "Slash", 53 },        { "ShiftRight", 54 },     { "NumpadMultiply", 55 }, { "AltLeft", 56 },
		{ "Space", 57 },        { "CapsLock", 58 },       { "F1", 59 },             { "F2", 60 },
		{ "F3", 61 },           { "F4", 62 },             { "F5", 63 },             { "F6", 64 },
		{ "F7", 65 },           { "F8", 66 },             { "F9", 67 },             { "F10", 68 },
		{ "NumLock", 69 },      { "ScrollLock", 70 },     { "Numpad7", 71 },        { "Numpad8", 72 },
		{ "Numpad9", 73 },      { "NumpadSubtract", 74 }, { "Numpad4", 75 },        { "Numpad5", 76 },
		{ "Numpad6", 77 },      { "NumpadAdd", 78 },      { "Numpad1", 79 },        { "Numpad2", 80 },
		{ "Numpad3", 81 },      { "Numpad0", 82 },        { "NumpadDecimal", 83 },  { "IntlBackslash", 86 },
		{ "F11", 87 },          { "F12", 88 },            { "NumpadEnter", 96 },    { "ControlRight", 97 },
		{ "NumpadDivide", 98 }, { "PrintScreen", 99 },    { "AltRight", 100 },      { "Home", 102 },
		{ "ArrowUp", 103 },     { "PageUp", 104 },        { "ArrowLeft", 105 },     { "ArrowRight", 106 },
		{ "End", 107 },         { "ArrowDown", 108 },     { "PageDown", 109 },      { "Insert", 110 },
		{ "Delete", 111 },      { "Pause", 119 },         { "MetaLeft", 125 },      { "MetaRight", 126 },
	};

	// page names for mouse buttons, X button number (DOM button 1, 3, 4 = X 2, 8, 9)
	inline constexpr std::pair< const char *, unsigned int > kMouse[] = {
		{ "MouseMiddle", 2 },
		{ "MouseBack", 8 },
		{ "MouseForward", 9 },
	};

	/** Mumble's button value for a DOM code or page mouse name; 0 if unknown */
	inline unsigned int toMumble(const QString &code) {
		for (const auto &k : kKeys) {
			if (code == QLatin1String(k.first)) {
				return k.second + kEvdevToX;
			}
		}
		for (const auto &m : kMouse) {
			if (code == QLatin1String(m.first)) {
				return m.second + kMouseBase;
			}
		}
		return 0;
	}

	/** the DOM code or page mouse name of a Mumble button value; empty if there is none */
	inline QString toCode(unsigned int button) {
		for (const auto &k : kKeys) {
			if (button == k.second + kEvdevToX) {
				return QLatin1String(k.first);
			}
		}
		for (const auto &m : kMouse) {
			if (button == m.second + kMouseBase) {
				return QLatin1String(m.first);
			}
		}
		return {};
	}

	/** keys that type or act on their own in other programs: the page warns before binding them (never blocks) */
	inline bool typesElsewhere(const QString &code) {
		static const char *const kQuiet[] = { "ControlRight", "AltRight", "Pause", "ScrollLock", "MetaRight",
											  "MouseMiddle", "MouseBack", "MouseForward" };
		for (const char *quiet : kQuiet) {
			if (code == QLatin1String(quiet)) {
				return false;
			}
		}
		// F13 and up are not in the table; F1 to F12 often have a meaning in the focused program
		return true;
	}

} // namespace pttkeys
} // namespace ruumbledesk

#endif
