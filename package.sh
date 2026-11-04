#!/usr/bin/env bash
# Builds Ruumble Desk packages in a container, so the host only needs Docker (any Linux).
#
#   ./package.sh package --distro <name>   the native package of that distro in dist/:
#                                            debian12, ubuntu2404 (also Mint 22)  .deb   sudo apt install ./dist/<file>
#                                            fedora (latest release)              .rpm   sudo dnf install ./dist/<file>
#                                            arch                                 .pkg.tar.zst   sudo pacman -U dist/<file>
#   ./package.sh appimage                  dist/Ruumble_Desk-x86_64.AppImage, one file for most distros
#                                          (built on Debian 12, glibc 2.36: Debian 12+, Ubuntu 22.10+, Fedora, Arch, ...)
#   ./package.sh all --distro <name>       both
#   ./package.sh deb --distro <name>       same as package (kept for the Debian ones)
#   ./package.sh every                     packages for all four distros plus the AppImage
#
# The repo (Ruumble at the root, Desk in desk/) and a local Mumble checkout are mounted read only; only dist/ is
# written. Without a local Mumble checkout the container clones it from MUMBLE_URL (pins.env).
set -eu
ROOT="$(cd "$(dirname "$0")" && pwd)"
source "$ROOT/tools/common.sh"
TARGET=${1:-}; shift || true
DISTRO=debian12
while [ $# -gt 0 ]; do
  case "$1" in --distro) DISTRO=$2; shift 2 ;; *) echo "unknown option $1"; exit 2 ;; esac
done
base_of() { # distro -> "dockerfile base-image"
  case "$1" in
    debian12) echo "Dockerfile debian:12" ;;
    ubuntu2404) echo "Dockerfile ubuntu:24.04" ;;
    fedora) echo "Dockerfile.fedora fedora:latest" ;;
    arch) echo "Dockerfile.arch archlinux:latest" ;;
    *) echo "unknown distro $1 (debian12, ubuntu2404, fedora, arch)" >&2; exit 2 ;;
  esac
}
[ "$TARGET" = deb ] && TARGET=package
case "$TARGET" in package|appimage|all|every) ;; *) sed -n '2,16p' "$0"; exit 2 ;; esac
command -v docker >/dev/null || { echo "Docker is needed"; exit 1; }

run() { # target distro
  local image="ruumble-desk-build:$2" spec
  spec=$(base_of "$2")
  echo "== $1 for $2 (${spec#* })"
  docker build -q -t "$image" -f "$ROOT/packaging/${spec%% *}" --build-arg BASE="${spec#* }" "$ROOT/packaging" >/dev/null
  mkdir -p "$ROOT/dist"
  local mumble=()  # a local Mumble checkout is used if there is one, otherwise the container clones MUMBLE_URL
  if src=$(abs "$MUMBLE_REPO") && [ -d "$src/.git" ]; then mumble=(-v "$src:/in/mumble:ro"); fi
  docker run --rm --user "$(id -u):$(id -g)" -e HOME=/tmp \
    -v "$RU:/in/repo:ro" "${mumble[@]}" \
    -v "$ROOT/dist:/out" "$image" /in/repo/desk/packaging/in-container.sh "$1" "$2"
}

case "$TARGET" in
  package) run package "$DISTRO" ;;
  appimage) run appimage debian12 ;;   # always from the oldest base, so it runs on as many systems as possible
  all) run package "$DISTRO"; run appimage debian12 ;;
  every) for d in debian12 ubuntu2404 fedora arch; do run package "$d"; done; run appimage debian12 ;;
esac
ls -la "$ROOT/dist"
