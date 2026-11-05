#!/usr/bin/env bash
# Moves Ruumble Client to another Mumble release.
#   tools/update-mumble.sh [tag]     tag e.g. v1.6.901; without one the latest Mumble release
#
# 1. MUMBLE_REF in pins.env (Ruumble's interface files are taken over in ruumble-server, then server/ is moved)
# 2. a fresh work/mumble at the tag (full clone, the 3-way merge needs the old tag's blobs)
# 3. patches/mumble applied with a 3-way merge
# 4. build, then tools/export-patches.sh so the patch fits the new tag exactly
#
# Exit codes: 0 done (review and commit: pins.env, patches, client),
#             3 the patch has conflicts, 4 the build failed. In both cases fix it in work/mumble (conflict markers,
#             build errors; the client code is in client/), then: tools/build.sh && tools/export-patches.sh
# Used the same way by .github/workflows/mumble-release-watch.yml.
set -euo pipefail
source "$(dirname "$0")/common.sh"

tag="${1:-}"
if [ -z "$tag" ]; then
  tag=$(curl -fsSL https://api.github.com/repos/mumble-voip/mumble/releases/latest | sed -nE 's/^ *"tag_name": *"([^"]+)".*/\1/p')
  [ -n "$tag" ] || { echo "could not read the latest Mumble release"; exit 1; }
fi
old="$MUMBLE_REF"
echo "== Mumble $old -> $tag"

sed -i -E "s/^MUMBLE_REF=.*/MUMBLE_REF=\${MUMBLE_REF:-$tag}/" "$ROOT/pins.env"
echo "== pins.env on $tag"

rm -rf "$WM" "$WB"
mkdir -p "$W"
src="$MUMBLE_URL"
if [ -n "$MUMBLE_REPO" ] && s=$(abs "$MUMBLE_REPO") && [ -d "$s/.git" ]; then src="$s"; fi
echo "== cloning Mumble from $src"
git clone -q "$src" "$WM"
git -C "$WM" fetch -q --tags "$MUMBLE_URL" 2>/dev/null || true
git -C "$WM" checkout -q -B ruumble-client "$tag"
git -C "$WM" submodule update --init --recursive -q
ln -sfn "$ROOT/client" "$WM/src/mumble/ruumble"

if ! apply_upstream > "$W/apply.log" 2>&1; then
  cat "$W/apply.log"
  echo
  echo "== a backport in patches/upstream no longer applies to $tag: if $tag has the fix in another form, remove it"
  echo "   from patches/upstream and run tools/update-mumble.sh $tag again"
  exit 3
fi
cat "$W/apply.log"

if ! git -C "$WM" apply --3way --whitespace=nowarn "$ROOT"/patches/mumble/*.patch > "$W/apply.log" 2>&1; then
  cat "$W/apply.log"
  echo
  echo "== the patch has conflicts in work/mumble (git -C work/mumble status)."
  echo "   Resolve them, then: tools/build.sh && tools/export-patches.sh"
  exit 3
fi
echo "== patch applied"

if ! BUILD_TYPE="${BUILD_TYPE:-RelWithDebInfo}" "$ROOT/tools/build.sh" > "$W/build.log" 2>&1; then
  tail -40 "$W/build.log"
  echo
  echo "== the build failed (log: $W/build.log). Fix it in work/mumble or client/, then:"
  echo "   tools/build.sh && tools/export-patches.sh"
  exit 4
fi
"$ROOT/tools/export-patches.sh"
echo "== Ruumble Client builds against Mumble $tag. Check the Mumble API that client/QtMumbleApi uses,"
echo "   start the client once (tools/run.sh), then commit."
