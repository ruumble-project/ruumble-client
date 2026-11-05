#!/usr/bin/env bash
# Starts Ruumble Client with your normal profile (~/.config/Ruumble/Ruumble Client). Arguments go to the client,
# e.g. a mumble:// URL; without one the server manager opens, as in Mumble.
set -eu
source "$(dirname "$0")/common.sh"
exec "$WB/mumble" "$@"
