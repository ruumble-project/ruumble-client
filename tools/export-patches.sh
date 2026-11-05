#!/usr/bin/env bash
# Writes the changes made in work/mumble back into patches/mumble (the client/ sources are edited in place via the
# symlink and need no export). Only our own changes: the diff starts after the upstream backports (tag ruumble-base).
# Run before committing.
set -eu
source "$(dirname "$0")/common.sh"

if [ -d "$WM/.git" ]; then
  other=$(git -C "$WM" status --porcelain --untracked-files=all | grep '^??' | grep -v ' src/mumble/ruumble' || true)
  [ -z "$other" ] || { echo "new files in work/mumble (put them into client/ or add them to the patch by hand):"; echo "$other"; exit 1; }
  base=$(git -C "$WM" rev-parse -q --verify ruumble-base || echo "$MUMBLE_REF")  # after the upstream backports
  git -C "$WM" diff "$base" -- . ':(exclude)src/mumble/ruumble' > "$ROOT/patches/mumble/0001-ruumble-desk-touch-points.patch"
  echo "patches/mumble: $(grep -c '^diff --git' "$ROOT/patches/mumble/0001-ruumble-desk-touch-points.patch") files"
fi
