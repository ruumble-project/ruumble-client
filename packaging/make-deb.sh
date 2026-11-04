#!/usr/bin/env bash
# Builds the .deb from work/build/mumble for the distro it runs on (dependencies via dpkg-shlibdeps).
#   make-deb.sh <distro-label> <out-dir>
set -eu
HERE="$(cd "$(dirname "$0")" && pwd)"
source "$HERE/../tools/common.sh"; source "$HERE/desktop-entry.sh"; source "$HERE/stage.sh"
LABEL=$1 OUT=$2
VERSION="$VERSION+$LABEL"
P="$W/deb/ruumble-desk"
rm -rf "$W/deb" && mkdir -p "$P/DEBIAN"
stage_files "$P"
mkdir -p "$P/usr/share/doc/ruumble-desk" && mv "$P/usr/share/licenses/ruumble-desk/NOTICE" "$P/usr/share/doc/ruumble-desk/copyright"
rm -r "$P/usr/share/licenses"

# shared-library dependencies, plus what is loaded at run time and so not visible to dpkg-shlibdeps
T=$(mktemp -d)
mkdir -p "$T/debian" && printf 'Source: ruumble-desk\n\nPackage: ruumble-desk\nArchitecture: any\n' > "$T/debian/control"
DEPS=$(cd "$T" && dpkg-shlibdeps -O -e "$P/usr/bin/ruumble-desk" 2>/dev/null | sed -n 's/^shlibs:Depends=//p')
rm -rf "$T"
RUNTIME="libqt6webenginecore6-bin, libqt6sql6-sqlite, qt6-qpa-plugins"

cat > "$P/DEBIAN/control" <<CTRL
Package: ruumble-desk
Version: $VERSION
Architecture: $(dpkg --print-architecture)
Maintainer: Kevin Messerschmidt <divzer0@proton.me>
Section: sound
Priority: optional
Depends: $DEPS, $RUNTIME
Recommends: pipewire-pulse | pulseaudio
Homepage: https://github.com/KevinWebman/Ruumble-Desk
Description: $DESCRIPTION_SHORT
$(echo "$DESCRIPTION_LONG" | fold -s -w 78 | sed 's/^/ /')
CTRL

mkdir -p "$OUT"
DEB="$OUT/ruumble-desk_${VERSION}_$(dpkg --print-architecture).deb"
dpkg-deb --root-owner-group --build "$P" "$DEB"
echo "package: $DEB"
