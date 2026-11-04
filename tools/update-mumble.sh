#!/usr/bin/env bash
# Moves Ruumble Desk (and Ruumble's interface files) to another Mumble release.
#   desk/tools/update-mumble.sh [tag]     tag e.g. v1.6.901; without one the latest Mumble release
#
# 1. tools/update-mumble-interfaces.sh <tag> (third_party/mumble) and MUMBLE_REF in desk/pins.env
# 2. a fresh work/mumble at the tag (full clone, the 3-way merge needs the old tag's blobs)
# 3. desk/patches/mumble applied with a 3-way merge
# 4. build, then tools/export-patches.sh so the patch fits the new tag exactly
#
# Exit codes: 0 done (review and commit: third_party/mumble, desk/pins.env, desk/patches, desk/client),
#             3 the patch has conflicts, 4 the build failed. In both cases fix it in work/mumble (conflict markers,
#             build errors; the client code is in desk/client), then: desk/tools/build.sh && desk/tools/export-patches.sh
# Used the same way by .github/workflows/mumble-release-watch.yml.
set -euo pipefail
source "$(dirname "$0")/common.sh"
REPO_ROOT="$RU"

tag="${1:-}"
if [ -z "$tag" ]; then
  tag=$(curl -fsSL https://api.github.com/repos/mumble-voip/mumble/releases/latest | sed -nE 's/^ *"tag_name": *"([^"]+)".*/\1/p')
  [ -n "$tag" ] || { echo "could not read the latest Mumble release"; exit 1; }
fi
old="$MUMBLE_REF"
echo "== Mumble $old -> $tag"

"$REPO_ROOT/tools/update-mumble-interfaces.sh" "$tag" > "$W/interfaces.diff" 2>&1 || { cat "$W/interfaces.diff"; exit 1; }
sed -i -E "s/^MUMBLE_REF=.*/MUMBLE_REF=\${MUMBLE_REF:-$tag}/" "$ROOT/pins.env"
echo "== interface files and desk/pins.env on $tag (diff: $W/interfaces.diff)"

rm -rf "$WM" "$WB"
mkdir -p "$W"
src="$MUMBLE_URL"
if [ -n "$MUMBLE_REPO" ] && s=$(abs "$MUMBLE_REPO") && [ -d "$s/.git" ]; then src="$s"; fi
echo "== cloning Mumble from $src"
git clone -q "$src" "$WM"
git -C "$WM" fetch -q --tags "$MUMBLE_URL" 2>/dev/null || true
git -C "$WM" checkout -q -B ruumble-desk "$tag"
git -C "$WM" submodule update --init --recursive -q
ln -sfn "$ROOT/client" "$WM/src/mumble/ruumble"

if ! git -C "$WM" apply --3way --whitespace=nowarn "$ROOT"/patches/mumble/*.patch > "$W/apply.log" 2>&1; then
  cat "$W/apply.log"
  echo
  echo "== the patch has conflicts in work/mumble (git -C desk/work/mumble status)."
  echo "   Resolve them, then: desk/tools/build.sh && desk/tools/export-patches.sh"
  exit 3
fi
echo "== patch applied"

if ! BUILD_TYPE="${BUILD_TYPE:-RelWithDebInfo}" "$ROOT/tools/build.sh" > "$W/build.log" 2>&1; then
  tail -40 "$W/build.log"
  echo
  echo "== the build failed (log: $W/build.log). Fix it in work/mumble or desk/client, then:"
  echo "   desk/tools/build.sh && desk/tools/export-patches.sh"
  exit 4
fi
"$ROOT/tools/export-patches.sh"
echo "== Ruumble Desk builds against Mumble $tag. Check docs/mumble-interfaces.md against $W/interfaces.diff,"
echo "   start the client once (desk/tools/run.sh), then commit."
