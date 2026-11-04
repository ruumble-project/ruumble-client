#!/usr/bin/env bash
# Builds Ruumble Desk in one go and puts the program into dist/.
#
#   ./build.sh              prepare work/ (first time), build, dist/ruumble-desk
#   ./build.sh --install    also a start menu entry "Ruumble Desk" and the command ruumble-desk (~/.local/bin)
#   ./build.sh --uninstall  removes the start menu entry and the command again (dist/ stays)
#   ./build.sh --appimage   also dist/Ruumble_Desk-x86_64.AppImage (one file for other Linux machines, tools/appimage.sh)
#
# The program uses the Qt libraries of this system (build dependencies: CLAUDE.md), so it runs here, not on any
# machine. Mumble and Ruumble are only read: tools/prepare.sh clones them into work/.
set -eu
ROOT="$(cd "$(dirname "$0")" && pwd)"
DIST="$ROOT/dist"
APPS="$HOME/.local/share/applications"
BIN="$HOME/.local/bin"
ICONS="$HOME/.local/share/icons/hicolor/scalable/apps"

uninstall() {
  rm -f "$APPS/ruumble-desk.desktop" "$BIN/ruumble-desk" "$ICONS/ruumble-desk.svg"
  command -v update-desktop-database >/dev/null && update-desktop-database "$APPS" 2>/dev/null || true
  echo "removed: start menu entry and command ruumble-desk"
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
install -m 755 "$ROOT/work/build/mumble" "$DIST/ruumble-desk"
install -m 644 "$ROOT/work/Ruumble/web/public/icon.svg" "$DIST/ruumble-desk.svg"
cat > "$DIST/ruumble-desk.desktop" <<EOF
[Desktop Entry]
Type=Application
Name=Ruumble Desk
GenericName=Voice chat with the Ruumble building
Comment=Mumble client with Ruumble built in
Exec="$DIST/ruumble-desk"
Icon=ruumble-desk
Terminal=false
Categories=Network;Chat;
StartupWMClass=ruumble-desk
EOF
echo "program: $DIST/ruumble-desk"

if [ "${1:-}" = --install ]; then
  mkdir -p "$APPS" "$BIN" "$ICONS"
  install -m 644 "$DIST/ruumble-desk.svg" "$ICONS/ruumble-desk.svg"
  install -m 644 "$DIST/ruumble-desk.desktop" "$APPS/ruumble-desk.desktop"
  ln -sfn "$DIST/ruumble-desk" "$BIN/ruumble-desk"
  command -v update-desktop-database >/dev/null && update-desktop-database "$APPS" 2>/dev/null || true
  echo "installed: start menu entry \"Ruumble Desk\", command ruumble-desk"
fi

if [ "${1:-}" = --appimage ]; then
  "$ROOT/tools/appimage.sh"
fi
