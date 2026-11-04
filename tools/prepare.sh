#!/usr/bin/env bash
# Creates the Mumble work tree: clones Mumble (from GitHub, or a local checkout named in MUMBLE_REPO), checks out the pinned
# ref and applies our patches. An existing work tree is left alone; remove it (rm -rf work/mumble) to start over,
# after tools/export-patches.sh if it holds changes you want to keep. Ruumble is the repo root and needs no work tree.
set -eu
source "$(dirname "$0")/common.sh"
mkdir -p "$W"

if [ ! -d "$WM/.git" ]; then
  if [ -n "$MUMBLE_REPO" ] && src=$(abs "$MUMBLE_REPO") && [ -d "$src/.git" ]; then
    echo "== Mumble $MUMBLE_REF from $src"
    git clone -q --no-checkout "$src" "$WM"
    git -C "$WM" checkout -q -B ruumble-desk "$MUMBLE_REF"
  else
    echo "== Mumble $MUMBLE_REF from $MUMBLE_URL"
    git clone -q --depth 1 --branch "$MUMBLE_REF" "$MUMBLE_URL" "$WM"
    git -C "$WM" checkout -q -B ruumble-desk
  fi
  git -C "$WM" submodule update --init --recursive -q
  git -C "$WM" apply --whitespace=nowarn "$ROOT"/patches/mumble/*.patch
else
  echo "== work/mumble exists, left as it is"
fi
# always, so a moved repo does not leave the work tree pointing at the old place
ln -sfn "$ROOT/client" "$WM/src/mumble/ruumble"
echo "ready: tools/build.sh"
