#!/usr/bin/env bash
# Builds the Arch Linux package (pkg.tar.zst) from work/build/mumble with makepkg. The dependencies are the packages
# that own the libraries the program links directly (readelf NEEDED, pacman -Qo), plus what is loaded at run time.
#   make-arch.sh <distro-label> <out-dir>
set -eu
HERE="$(cd "$(dirname "$0")" && pwd)"
source "$HERE/../tools/common.sh"; source "$HERE/desktop-entry.sh"; source "$HERE/stage.sh"
LABEL=$1 OUT=$2
A="$W/arch"
rm -rf "$A" && mkdir -p "$A/stage"
stage_files "$A/stage"
# only the libraries the program links directly (NEEDED), resolved to their paths, then to the owning packages
NEEDED=$(readelf -d "$WB/mumble" | sed -n 's/.*Shared library: \[\(.*\)\]/\1/p')
DEPS=$(ldd "$WB/mumble" | awk 'NR==FNR{need[$1]=1; next} ($1 in need) && /=> \//{print $3}' <(echo "$NEEDED") - \
  | xargs -r pacman -Qoq 2>/dev/null | sort -u | grep -v '^glibc$\|^gcc-libs$' | tr '\n' ' ')
DEPS="$DEPS glibc gcc-libs qt6-webengine"
cat > "$A/PKGBUILD" <<PKG
pkgname=ruumble-client
pkgver=$VERSION
pkgrel=1
pkgdesc="$DESCRIPTION_SHORT"
arch=(x86_64)
url="https://github.com/ruumble-project/ruumble-client"
license=(BSD-3-Clause LGPL-3.0-only)
depends=($(echo $DEPS | tr ' ' '\n' | sort -u | tr '\n' ' '))
optdepends=('pipewire-pulse: audio through PipeWire')
provides=(ruumble-desk)
conflicts=(ruumble-desk)
replaces=(ruumble-desk)
options=(!strip !debug)
package() {
  cp -a "$A/stage/." "\$pkgdir/"
}
PKG
(cd "$A" && PKGDEST="$OUT" makepkg --nodeps --force --noconfirm)
echo "package: $(ls "$OUT"/ruumble-client-"$VERSION"-*.pkg.tar.zst)"
