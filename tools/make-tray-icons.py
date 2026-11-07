#!/usr/bin/env python3
# Writes the tray icons of Ruumble Client into client/tray: one sign per state of Mumble's tray icon, in the style of
# the signs in Ruumble's top bar (rounded plate, dark rim, screws from 32 px on) with one Lucide glyph (ISC, see
# client/tray/LICENSE.lucide) each. Every state gets <name>.svg (32 px and up, with screws) and <name>-small.svg
# (16 to 24 px, without them). White: set by the user; sky: talking; red: by the server; grey: suppressed; brass: push
# to mute. Run again only to change them; client/TrayIcons.h loads them.
from pathlib import Path

OUT = Path(__file__).resolve().parent.parent / "client" / "tray"

INK, SKY, PLATE, YELLOW, ALERT, BRASS, GREY, WHITE = "#003869", "#5aa6e7", "#e7f1fb", "#fbd200", "#d2463c", "#b98a2e", "#7d8995", "#ffffff"

GLYPHS = {
    "mic": '<path d="M12 19v3"/><path d="M19 10v2a7 7 0 0 1-14 0v-2"/><rect x="9" y="2" width="6" height="13" rx="3"/>',
    "mic-off": '<path d="M2 2l20 20"/><path d="M18.89 13.23A7.12 7.12 0 0 0 19 12v-2"/><path d="M5 10v2a7 7 0 0 0 12 5"/>'
    '<path d="M15 9.34V5a3 3 0 0 0-5.68-1.33"/><path d="M9 9v3a3 3 0 0 0 5.12 2.12"/><path d="M12 19v3"/>',
    "audio": '<path d="M2 10v3"/><path d="M6 6v11"/><path d="M10 3v18"/><path d="M14 8v7"/><path d="M18 5v13"/><path d="M22 10v3"/>',
    "megaphone": '<path d="m3 11 18-5v12L3 14v-3z"/><path d="M11.6 16.8a3 3 0 1 1-5.8-1.6"/>',
    "headphone-off": '<path d="M21 14h-1.343"/><path d="M9.128 3.47A9 9 0 0 1 21 12v3.343"/><path d="M2 2l20 20"/>'
    '<path d="M20.414 20.414A2 2 0 0 1 19 21h-1a2 2 0 0 1-2-2v-3"/>'
    '<path d="M3 14h3a2 2 0 0 1 2 2v3a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-7a9 9 0 0 1 2.636-6.364"/>',
    "bubble": '<path d="M7.9 20A9 9 0 1 0 4 16.1L2 22Z"/>',
}

# file name: (plate, glyph colour, glyph, dashed rim); the names follow Mumble's fields in MainWindow
SIGNS = {
    "talking-off": (WHITE, INK, "mic", False),
    "talking-on": (SKY, INK, "audio", False),
    "talking-whisper": (PLATE, INK, "audio", True),
    "talking-shout": (SKY, INK, "megaphone", False),
    "muted-self": (WHITE, INK, "mic-off", False),
    "muted-server": (ALERT, WHITE, "mic-off", False),
    "muted-suppressed": (GREY, WHITE, "mic-off", False),
    "muted-push-to-mute": (BRASS, WHITE, "mic-off", False),
    "deafened-self": (WHITE, INK, "headphone-off", False),
    "deafened-server": (ALERT, WHITE, "headphone-off", False),
    "information": (YELLOW, INK, "bubble", False),
}


def sign(plate, fg, glyph, dashed, screws):
    rim = ' stroke-dasharray="10 6"' if dashed else ""
    dots = "".join(f'<circle cx="{x}" cy="{y}" r="1.8" fill="{fg}" opacity="0.55"/>' for x, y in ((13, 13), (51, 13), (13, 51), (51, 51))) if screws else ""
    return (
        '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64">'
        f'<rect x="3" y="3" width="58" height="58" rx="12" fill="{plate}" stroke="{INK}" stroke-width="5"{rim}/>{dots}'
        f'<g transform="translate(14 14) scale(1.5)" fill="none" stroke="{fg}" stroke-width="2.4" stroke-linecap="round" stroke-linejoin="round">{GLYPHS[glyph]}</g>'
        "</svg>\n"
    )


def app(small):
    # Ruumble's icon (server/web/public/icon.svg); small: thicker lines and dot, so they stay visible at 16 px
    width, dot = (5, 6) if small else (3, 4)
    return (
        '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64"><rect width="64" height="64" rx="8" fill="#003869"/>'
        f'<g fill="none" stroke="#fff" stroke-width="{width}"><rect x="12" y="12" width="40" height="40"/><path d="M12 32h40M32 12v14M32 38v14"/></g>'
        f'<circle cx="22" cy="22" r="{dot}" fill="#fbd200"/></svg>\n'
    )


OUT.mkdir(parents=True, exist_ok=True)
for name, (plate, fg, glyph, dashed) in SIGNS.items():
    (OUT / f"{name}.svg").write_text(sign(plate, fg, glyph, dashed, True))
    (OUT / f"{name}-small.svg").write_text(sign(plate, fg, glyph, dashed, False))
(OUT / "app.svg").write_text(app(False))
(OUT / "app-small.svg").write_text(app(True))
print(f"wrote {len(SIGNS) * 2 + 2} icons to {OUT}")
