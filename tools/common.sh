# shared by the tools: repo root, pins, work dirs
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# shellcheck source=../pins.env
source "$ROOT/pins.env"
abs() { case "$1" in /*) echo "$1" ;; *) (cd "$ROOT/$1" && pwd) ;; esac; }
W="$ROOT/work"
WM="$W/mumble"     # Mumble at MUMBLE_REF + patches/mumble + client/ (symlink)
WR="$W/Ruumble"    # Ruumble at RUUMBLE_REF + patches/ruumble; Mumble's CMake finds it as ../Ruumble
WB="$W/build"      # build dir of the client
