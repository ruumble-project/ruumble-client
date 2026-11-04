#!/usr/bin/env bash
# Packs the built client (work/build/mumble) into dist/Ruumble_Desk-x86_64.AppImage: one file with Qt, Qt WebEngine
# and all libraries, runs on other Linux machines with the same or a newer glibc than this one (2.39 here).
# Uses linuxdeploy with its Qt plugin, pinned versions, downloaded into work/appimage/tools on first use.
set -eu
source "$(dirname "$0")/common.sh"

LINUXDEPLOY_TAG="1-alpha-20251107-1"
PLUGIN_QT_TAG="1-alpha-20250213-1"
A="$W/appimage"
TOOLS="$A/tools"
APPDIR="$A/AppDir"
[ -x "$WB/mumble" ] || { echo "build first: ./build.sh"; exit 1; }

mkdir -p "$TOOLS"
fetch() { # name url
  [ -x "$TOOLS/$1" ] && return
  echo "== downloading $1"
  curl -fsSL -o "$TOOLS/$1.part" "$2"
  chmod +x "$TOOLS/$1.part"
  mv "$TOOLS/$1.part" "$TOOLS/$1"
}
fetch linuxdeploy-x86_64.AppImage \
  "https://github.com/linuxdeploy/linuxdeploy/releases/download/$LINUXDEPLOY_TAG/linuxdeploy-x86_64.AppImage"
fetch linuxdeploy-plugin-qt-x86_64.AppImage \
  "https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/$PLUGIN_QT_TAG/linuxdeploy-plugin-qt-x86_64.AppImage"

# a fresh AppDir every time
rm -rf "$APPDIR"
mkdir -p "$APPDIR/usr/bin" "$A/meta"
install -m 755 "$WB/mumble" "$APPDIR/usr/bin/ruumble-desk"
install -m 644 "$WR/web/public/icon.svg" "$A/meta/ruumble-desk.svg"
cat > "$A/meta/ruumble-desk.desktop" <<'EOF'
[Desktop Entry]
Type=Application
Name=Ruumble Desk
GenericName=Voice chat with the Ruumble building
Comment=Mumble client with Ruumble built in
Exec=ruumble-desk
Icon=ruumble-desk
Terminal=false
Categories=Network;Chat;
StartupWMClass=ruumble-desk
EOF

mkdir -p "$ROOT/dist"
cd "$ROOT/dist"
# Qt 6 from the system; the plugin finds WebEngine (QtWebEngineProcess, resources, locales) through the linked libs.
# Mumble also needs the SQLite driver (its database) and the OpenSSL TLS backend (server connection).
export QMAKE=/usr/lib/qt6/bin/qmake6
export EXTRA_QT_PLUGINS="sqldrivers;tls;iconengines"
export ARCH=x86_64
export LDAI_OUTPUT="Ruumble_Desk-x86_64.AppImage"
export PATH="$TOOLS:$PATH" # linuxdeploy looks for linuxdeploy-plugin-qt-*.AppImage on PATH
"$TOOLS/linuxdeploy-x86_64.AppImage" --appdir "$APPDIR" \
  --executable "$APPDIR/usr/bin/ruumble-desk" \
  --desktop-file "$A/meta/ruumble-desk.desktop" \
  --icon-file "$A/meta/ruumble-desk.svg" \
  --plugin qt --output appimage
echo "AppImage: $ROOT/dist/$LDAI_OUTPUT"
