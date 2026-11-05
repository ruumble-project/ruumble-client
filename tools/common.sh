# shared by the tools: repo root, Ruumble (the server submodule), pins, work dirs
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# shellcheck source=../pins.env
source "$ROOT/pins.env"
abs() { case "$1" in /*) echo "$1" ;; *) (cd "$ROOT/$1" 2>/dev/null && pwd) ;; esac; }
RU="$ROOT/server"   # Ruumble: ruumble-server as a git submodule (plugin core, icon, service for the local stack)
[ -f "$RU/plugin/CMakeLists.txt" ] || { echo "server/ is empty: git submodule update --init" >&2; exit 1; }
W="$ROOT/work"
WM="$W/mumble"     # Mumble at MUMBLE_REF + patches/mumble + client/ (symlink)
WB="$W/build"      # build dir of the client
VERSION="$(cat "$ROOT/VERSION")"  # Ruumble Desk's own version (tools/release.sh)
