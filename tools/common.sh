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
VERSION="$(cat "$ROOT/VERSION")"  # Ruumble Client's own version (tools/release.sh)

# Fixes for Mumble itself that are not at MUMBLE_REF (patches/upstream): backports from its master (e.g. the OpenSSL 4
# build fix Fedora needs) and our own general fixes meant to go upstream (SRV priority, ticket #14). Applied before our
# touch points and committed in work/mumble, tagged ruumble-base, so that
# tools/export-patches.sh writes only our own changes. A fix the Mumble tag already contains is skipped.
apply_upstream() {
  local p applied=0
  for p in "$ROOT"/patches/upstream/*.patch; do
    [ -e "$p" ] || continue
    if git -C "$WM" apply --reverse --check "$p" 2>/dev/null; then
      echo "== upstream $(basename "$p"): already in Mumble, skipped (remove it from patches/upstream)"
      continue
    fi
    git -C "$WM" apply --whitespace=nowarn "$p" || return 1
    echo "== upstream $(basename "$p")"
    applied=1
  done
  if [ "$applied" = 1 ]; then
    git -C "$WM" -c user.name=ruumble -c user.email=ruumble@localhost commit -q -a -m "Upstream backports (patches/upstream)"
  fi
  git -C "$WM" tag -f ruumble-base >/dev/null
}
