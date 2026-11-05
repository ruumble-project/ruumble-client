#!/usr/bin/env bash
# Runs inside the packaging container (started by ./package.sh). Read only: /in/repo (this repo),
# /in/mumble (optional). Writes only to /out (dist/ of the host) and to its own scratch dir.
#   in-container.sh <package|appimage> <distro>   (package = the native package of the container's distro)
set -eu
TARGET=$1 DISTRO=$2
S=/tmp/rd            # scratch: a copy of this repo, the Mumble work tree, the build
rm -rf "$S" && mkdir -p "$S/repo"
(cd /in/repo && tar --exclude=./.git --exclude=node_modules --exclude=./work \
  --exclude=./dist --exclude=./test/.work -cf - .) | (cd "$S/repo" && tar -xf -)
git config --global --add safe.directory '*'

cd "$S/repo"
tools/prepare.sh
BUILD_TYPE=Release tools/build.sh

case "$TARGET" in
  package)
    . /etc/os-release
    case "$ID" in
      debian|ubuntu) packaging/make-deb.sh "$DISTRO" /out ;;
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
