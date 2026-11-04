#!/usr/bin/env bash
# Builds the Ruumble Desk client (client only, no server) into work/build.
set -eu
source "$(dirname "$0")/common.sh"
[ -d "$WM/.git" ] || "$ROOT/tools/prepare.sh"
if [ ! -f "$WB/CMakeCache.txt" ]; then
  cmake -S "$WM" -B "$WB" -G Ninja -DCMAKE_BUILD_TYPE="${BUILD_TYPE:-RelWithDebInfo}" \
    -Dserver=OFF -Dtests=OFF -Doverlay-xcompile=OFF -Dzeroconf=OFF -Dspeechd=OFF \
    -Djackaudio=OFF -Dportaudio=OFF -Dupdate=OFF -Dtranslations=OFF -Dwarnings-as-errors=OFF \
    -DRUUMBLE_SOURCE_DIR="$RU"
fi
cmake --build "$WB" -j"$(nproc)"
echo "built: $WB/mumble   (start: tools/run.sh)"
