#!/usr/bin/env bash
# Creates the work trees: clones the original repos (read only, nothing is written there), checks out the pinned
# refs and applies our patches. An existing work tree is left alone; remove it (rm -rf work/mumble or work/Ruumble)
# to start over, after tools/export-patches.sh if it holds changes you want to keep.
set -eu
source "$(dirname "$0")/common.sh"
mkdir -p "$W"

if [ ! -d "$WM/.git" ]; then
  echo "== Mumble $MUMBLE_REF from $(abs "$MUMBLE_REPO")"
  git clone -q --no-checkout "$(abs "$MUMBLE_REPO")" "$WM"
  git -C "$WM" checkout -q -B ruumble-desk "$MUMBLE_REF"
  git -C "$WM" submodule update --init --recursive -q
  git -C "$WM" apply --whitespace=nowarn "$ROOT"/patches/mumble/*.patch
  ln -sfn "$ROOT/client" "$WM/src/mumble/ruumble"
else
  echo "== work/mumble exists, left as it is"
fi

if [ ! -d "$WR/.git" ]; then
  echo "== Ruumble $RUUMBLE_REF from $(abs "$RUUMBLE_REPO")"
  git clone -q --no-checkout "$(abs "$RUUMBLE_REPO")" "$WR"
  git -C "$WR" checkout -q -B ruumble-desk "$RUUMBLE_REF"
  git -C "$WR" apply --whitespace=nowarn "$ROOT"/patches/ruumble/*.patch
else
  echo "== work/Ruumble exists, left as it is"
fi
echo "ready: tools/build.sh"
