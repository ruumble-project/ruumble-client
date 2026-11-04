#!/usr/bin/env bash
# Runs inside the packaging container (started by ./package.sh). Read only: /in/ruumble-desk, /in/mumble,
# /in/Ruumble. Writes only to /out (dist/ of the host) and to its own scratch dir.
#   in-container.sh <package|appimage> <distro>   (package = the native format of the container's distro)
set -eu
TARGET=$1 DISTRO=$2
S=/tmp/rd            # scratch: a copy of this repo, the work trees, the build
rm -rf "$S" && mkdir -p "$S/src"
(cd /in/ruumble-desk && tar --exclude=./work --exclude=./dist --exclude=./test/.work -cf - .) | (cd "$S/src" && tar -xf -)
# pins.env names ../mumble and ../Ruumble: put the read-only mounts there
ln -sfn /in/mumble "$S/mumble"
ln -sfn /in/Ruumble "$S/Ruumble"
sed -i 's|^MUMBLE_REPO=.*|MUMBLE_REPO=../mumble|; s|^RUUMBLE_REPO=.*|RUUMBLE_REPO=../Ruumble|' "$S/src/pins.env"
git config --global --add safe.directory '*'

cd "$S/src"
tools/prepare.sh
BUILD_TYPE=Release tools/build.sh

case "$TARGET" in
  package)
    . /etc/os-release
    case "$ID" in
      debian|ubuntu) packaging/make-deb.sh "$DISTRO" /out ;;
      fedora) packaging/make-rpm.sh "fc${VERSION_ID}" /out ;;
      arch) packaging/make-arch.sh "$DISTRO" /out ;;
      *) echo "no package format for $ID"; exit 1 ;;
    esac
    ;;
  appimage)
    export APPIMAGE_EXTRACT_AND_RUN=1 # no FUSE in a container
    tools/appimage.sh
    cp dist/*.AppImage /out/
    ;;
esac
