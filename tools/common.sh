# shared by the tools: desk root, repo root (Ruumble), pins, work dirs
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# shellcheck source=../pins.env
source "$ROOT/pins.env"
abs() { case "$1" in /*) echo "$1" ;; *) (cd "$ROOT/$1" 2>/dev/null && pwd) ;; esac; }
RU="$(cd "$ROOT/.." && pwd)"   # Ruumble: the repo root (plugin core, web UI, service)
W="$ROOT/work"
WM="$W/mumble"     # Mumble at MUMBLE_REF + patches/mumble + client/ (symlink)
WB="$W/build"      # build dir of the client
VERSION="$(sed -nE 's/^ *"version": *"([^"]+)".*/\1/p' "$RU/bridge/package.json")"  # one version for Ruumble and Desk
