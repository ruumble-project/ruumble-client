# shared by the package scripts: installs the program, icon, desktop entry and notice under a root dir ($1)
stage_files() {
  local P=$1
  mkdir -p "$P/usr/bin" "$P/usr/share/applications" "$P/usr/share/icons/hicolor/scalable/apps" "$P/usr/share/licenses/ruumble-desk"
  install -m 755 "$WB/mumble" "$P/usr/bin/ruumble-desk"
  strip --strip-unneeded "$P/usr/bin/ruumble-desk"
  install -m 644 "$WR/web/public/icon.svg" "$P/usr/share/icons/hicolor/scalable/apps/ruumble-desk.svg"
  desktop_entry > "$P/usr/share/applications/ruumble-desk.desktop"
  desktop-file-validate "$P/usr/share/applications/ruumble-desk.desktop"
  cat > "$P/usr/share/licenses/ruumble-desk/NOTICE" <<'COPY'
Ruumble Desk: Mumble (BSD-3-Clause, https://github.com/mumble-voip/mumble) with Ruumble (BSD-3-Clause) built in.
Uses Qt and Qt WebEngine (LGPL-3.0, Chromium) from the distribution.
COPY
}
DESCRIPTION_SHORT="Mumble client with the Ruumble office building built in"
DESCRIPTION_LONG="Ruumble Desk is the Mumble voice client with Ruumble's office-building view as its whole window: rooms are channels, a double-click moves you there, the Mumble log, channel chat and menus are part of the building. Settings and certificate are kept apart from the official Mumble client."
