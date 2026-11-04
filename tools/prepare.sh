#!/usr/bin/env bash
# Creates the Mumble work tree: clones Mumble (read only, nothing is written to the source), checks out the pinned
# ref and applies our patches. An existing work tree is left alone; remove it (rm -rf work/mumble) to start over,
# after tools/export-patches.sh if it holds changes you want to keep. Ruumble is the repo root and needs no work tree.
set -eu
source "$(dirname "$0")/common.sh"
mkdir -p "$W"

if [ ! -d "$WM/.git" ]; then
  if src=$(abs "$MUMBLE_REPO") && [ -d "$src/.git" ]; then
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
  ln -sfn "$ROOT/client" "$WM/src/mumble/ruumble"
else
  echo "== work/mumble exists, left as it is"
fi
echo "ready: tools/build.sh"
