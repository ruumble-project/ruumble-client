#!/usr/bin/env bash
# Builds the .deb from work/build/mumble for the distro it runs on (dependencies via dpkg-shlibdeps).
#   make-deb.sh <distro-label> <out-dir>
set -eu
HERE="$(cd "$(dirname "$0")" && pwd)"
source "$HERE/../tools/common.sh"; source "$HERE/desktop-entry.sh"; source "$HERE/stage.sh"
LABEL=$1 OUT=$2
VERSION="$VERSION+$LABEL"
P="$W/deb/ruumble-client"
rm -rf "$W/deb" && mkdir -p "$P/DEBIAN"
stage_files "$P"
mkdir -p "$P/usr/share/doc/ruumble-client" && mv "$P/usr/share/licenses/ruumble-client/NOTICE" "$P/usr/share/doc/ruumble-client/copyright"
rm -r "$P/usr/share/licenses"

# shared-library dependencies, plus what is loaded at run time and so not visible to dpkg-shlibdeps
T=$(mktemp -d)
mkdir -p "$T/debian" && printf 'Source: ruumble-client\n\nPackage: ruumble-client\nArchitecture: any\n' > "$T/debian/control"
DEPS=$(cd "$T" && dpkg-shlibdeps -O -e "$P/usr/bin/ruumble-client" 2>/dev/null | sed -n 's/^shlibs:Depends=//p')
rm -rf "$T"
RUNTIME="libqt6webenginecore6-bin, libqt6sql6-sqlite, qt6-qpa-plugins"

cat > "$P/DEBIAN/control" <<CTRL
Package: ruumble-client
Version: $VERSION
Architecture: $(dpkg --print-architecture)
Maintainer: Kevin Messerschmidt <divzer0@proton.me>
Section: sound
Priority: optional
Depends: $DEPS, $RUNTIME
Recommends: pipewire-pulse | pulseaudio
Replaces: ruumble-desk
Breaks: ruumble-desk
Provides: ruumble-desk
Homepage: https://github.com/ruumble-project/ruumble-client
Description: $DESCRIPTION_SHORT
$(echo "$DESCRIPTION_LONG" | fold -s -w 78 | sed 's/^/ /')
CTRL

mkdir -p "$OUT"
DEB="$OUT/ruumble-client_${VERSION}_$(dpkg --print-architecture).deb"
dpkg-deb --root-owner-group --build "$P" "$DEB"
echo "package: $DEB"
