#!/usr/bin/env bash
# Writes the changes made in the work trees back into patches/ (the client/ sources are edited in place via the
# symlink and need no export). Run before committing.
set -eu
source "$(dirname "$0")/common.sh"

if [ -d "$WM/.git" ]; then
  other=$(git -C "$WM" status --porcelain --untracked-files=all | grep '^??' | grep -v ' src/mumble/ruumble' || true)
  [ -z "$other" ] || { echo "new files in work/mumble (put them into client/ or add them to the patch by hand):"; echo "$other"; exit 1; }
  git -C "$WM" diff "$MUMBLE_REF" -- . ':(exclude)src/mumble/ruumble' > "$ROOT/patches/mumble/0001-ruumble-desk-touch-points.patch"
  echo "patches/mumble: $(grep -c '^diff --git' "$ROOT/patches/mumble/0001-ruumble-desk-touch-points.patch") files"
fi

if [ -d "$WR/.git" ]; then
  git -C "$WR" add -A -N .
  git -C "$WR" diff "$RUUMBLE_REF" > "$ROOT/patches/ruumble/0001-desk-panel.patch"
  git -C "$WR" reset -q
  echo "patches/ruumble: $(grep -c '^diff --git' "$ROOT/patches/ruumble/0001-desk-panel.patch") files"
fi
