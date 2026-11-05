# shared by the package scripts: installs the program, icon, desktop entry and notice under a root dir ($1)
stage_files() {
  local P=$1
  mkdir -p "$P/usr/bin" "$P/usr/share/applications" "$P/usr/share/icons/hicolor/scalable/apps" "$P/usr/share/licenses/ruumble-client"
  install -m 755 "$WB/mumble" "$P/usr/bin/ruumble-client"
  strip --strip-unneeded "$P/usr/bin/ruumble-client"
  install -m 644 "$RU/web/public/icon.svg" "$P/usr/share/icons/hicolor/scalable/apps/ruumble-client.svg"
  desktop_entry > "$P/usr/share/applications/ruumble-client.desktop"
  desktop-file-validate "$P/usr/share/applications/ruumble-client.desktop"
  cat > "$P/usr/share/licenses/ruumble-client/NOTICE" <<'COPY'
Ruumble Client: Mumble (BSD-3-Clause, https://github.com/mumble-voip/mumble) with Ruumble (BSD-3-Clause) built in.
Uses Qt and Qt WebEngine (LGPL-3.0, Chromium) from the distribution.
Contains the popover polyfill by OddBird (BSD-3-Clause, https://github.com/oddbird/popover-polyfill).
COPY
}
DESCRIPTION_SHORT="Mumble client with the Ruumble office building built in"
DESCRIPTION_LONG="Ruumble Client is the Mumble voice client with Ruumble's office-building view as its whole window: rooms are channels, a double-click moves you there, the Mumble log, channel chat and menus are part of the building. Settings and certificate are kept apart from the official Mumble client."
