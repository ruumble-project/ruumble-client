#!/usr/bin/env bash
# Builds the Ruumble Client client (client only, no server) into work/build.
set -eu
source "$(dirname "$0")/common.sh"
"$ROOT/tools/prepare.sh" >/dev/null # creates the work tree once, refreshes the client link
# a build dir configured at another path (the repo was moved) cannot regenerate itself: start it over
if [ -f "$WB/CMakeCache.txt" ] && ! grep -qxF "CMAKE_HOME_DIRECTORY:INTERNAL=$WM" "$WB/CMakeCache.txt"; then
  echo "== work/build was configured elsewhere, configuring again"
  rm -rf "$WB"
fi
if [ ! -f "$WB/CMakeCache.txt" ]; then
  cmake -S "$WM" -B "$WB" -G Ninja -DCMAKE_BUILD_TYPE="${BUILD_TYPE:-RelWithDebInfo}" \
    -Dserver=OFF -Dtests=OFF -Doverlay-xcompile=OFF -Dzeroconf=OFF -Dspeechd=OFF \
    -Djackaudio=OFF -Dportaudio=OFF -Dupdate=OFF -Dtranslations=OFF -Dwarnings-as-errors=OFF \
    -DRUUMBLE_SOURCE_DIR="$RU"
fi
cmake --build "$WB" -j"$(nproc)"
echo "built: $WB/mumble   (start: tools/run.sh)"
