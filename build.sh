#!/usr/bin/env bash
# Builds Ruumble Client in one go and puts the program into dist/.
#
#   ./build.sh              prepare work/ (first time), build, dist/ruumble-client
#   ./build.sh --install    also a start menu entry "Ruumble Client" and the command ruumble-client (~/.local/bin)
#   ./build.sh --uninstall  removes the start menu entry and the command again (dist/ stays)
#   ./build.sh --appimage   also dist/Ruumble_Client-x86_64.AppImage (one file for other Linux machines, tools/appimage.sh)
#
# The program uses the Qt libraries of this system (build dependencies: CLAUDE.md), so it runs here, not on any
# machine. Mumble is only read: tools/prepare.sh clones it into work/; Ruumble is the submodule server/.
set -eu
ROOT="$(cd "$(dirname "$0")" && pwd)"
DIST="$ROOT/dist"
APPS="$HOME/.local/share/applications"
BIN="$HOME/.local/bin"
ICONS="$HOME/.local/share/icons/hicolor/scalable/apps"

remove_old_name() { # entries of the former name Ruumble Desk
  rm -f "$APPS/ruumble-desk.desktop" "$BIN/ruumble-desk" "$ICONS/ruumble-desk.svg"
}

uninstall() {
  rm -f "$APPS/ruumble-client.desktop" "$BIN/ruumble-client" "$ICONS/ruumble-client.svg"
  remove_old_name
  command -v update-desktop-database >/dev/null && update-desktop-database "$APPS" 2>/dev/null || true
  echo "removed: start menu entry and command ruumble-client"
}

case "${1:-}" in
  --uninstall) uninstall; exit 0 ;;
  ""|--install|--appimage) ;;
  *) echo "usage: $0 [--install|--uninstall|--appimage]"; exit 2 ;;
esac

"$ROOT/tools/prepare.sh"
"$ROOT/tools/build.sh"

# dist/: the program, its icon and a desktop entry pointing at it
mkdir -p "$DIST"
install -m 755 "$ROOT/work/build/mumble" "$DIST/ruumble-client"
install -m 644 "$ROOT/server/web/public/icon.svg" "$DIST/ruumble-client.svg"
cat > "$DIST/ruumble-client.desktop" <<EOF
[Desktop Entry]
Type=Application
Name=Ruumble Client
GenericName=Voice chat with the Ruumble building
Comment=Mumble client with Ruumble built in
Exec="$DIST/ruumble-client"
Icon=ruumble-client
Terminal=false
Categories=Network;Chat;
StartupWMClass=ruumble-client
EOF
echo "program: $DIST/ruumble-client"

if [ "${1:-}" = --install ]; then
  mkdir -p "$APPS" "$BIN" "$ICONS"
  remove_old_name
  install -m 644 "$DIST/ruumble-client.svg" "$ICONS/ruumble-client.svg"
  install -m 644 "$DIST/ruumble-client.desktop" "$APPS/ruumble-client.desktop"
  ln -sfn "$DIST/ruumble-client" "$BIN/ruumble-client"
  command -v update-desktop-database >/dev/null && update-desktop-database "$APPS" 2>/dev/null || true
  # an icon cache older than the icon hides it from the menu (Cinnamon, GNOME): refresh it
  command -v gtk-update-icon-cache >/dev/null && gtk-update-icon-cache -q -f -t "${ICONS%/scalable/apps}" || true
  echo "installed: start menu entry \"Ruumble Client\", command ruumble-client"
fi

if [ "${1:-}" = --appimage ]; then
  "$ROOT/tools/appimage.sh"
fi
