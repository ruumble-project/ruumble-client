#!/usr/bin/env bash
# Windows build of Ruumble Client, run in Git Bash on a GitHub Windows runner (.github/workflows/windows.yml):
#   packaging/windows/build.sh
# Needs the MSVC environment (msvc-dev-cmd), Ninja, Qt with WebEngine in QT_ROOT_DIR (install-qt-action) and vcpkg in
# VCPKG_INSTALLATION_ROOT; the other libraries come from vcpkg in manifest mode (packaging/windows/vcpkg.json).
# Mumble supports only static builds on Windows, but Qt WebEngine exists only as shared libraries, so this is a
# shared build of its own. Result: dist/Ruumble_Client-x86_64-windows.zip, a folder with ruumble-client.exe, its DLLs
# and Qt WebEngine. xboxinput and gkey stay on (no libraries needed): Mumble's
# GlobalShortcut_win.h gets windows.h only through XboxInput.h.
# Logs: work/configure.log, work/build.log, work/deploy.log.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
source "$ROOT/tools/common.sh"
: "${VCPKG_INSTALLATION_ROOT:?vcpkg is needed}" "${QT_ROOT_DIR:?Qt is needed (QT_ROOT_DIR)}"
win() { cygpath -m "$1"; }   # paths for the Windows tools

"$ROOT/tools/prepare.sh"

if [ ! -f "$WB/CMakeCache.txt" ]; then
  cmake -S "$(win "$WM")" -B "$(win "$WB")" -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE="$(win "$VCPKG_INSTALLATION_ROOT")/scripts/buildsystems/vcpkg.cmake" \
    -DVCPKG_MANIFEST_DIR="$(win "$ROOT/packaging/windows")" -DVCPKG_INSTALLED_DIR="$(win "$W/vcpkg_installed")" \
    -DVCPKG_TARGET_TRIPLET=x64-windows -DCMAKE_PREFIX_PATH="$(win "$QT_ROOT_DIR")" \
    -Dserver=OFF -Dtests=OFF -Dplugins=OFF -Doverlay=OFF -Dpackaging=OFF -Dupdate=OFF -Dtranslations=OFF \
    -Dwarnings-as-errors=OFF -Dlto=OFF -Dzeroconf=OFF -Djackaudio=OFF -Dportaudio=OFF -Dasio=OFF -Dg15=OFF \
    -Dcrash-report=OFF -Delevation=OFF \
    -DRUUMBLE_SOURCE_DIR="$(win "$RU")" 2>&1 | tee "$W/configure.log"
fi
cmake --build "$(win "$WB")" 2>&1 | tee "$W/build.log"

# the folder people unpack: the program under its own name, the DLLs from vcpkg and the bundled codecs (next to
# mumble.exe in the build dir), Qt with WebEngine (windeployqt) and the MSVC run time
D="$W/win/Ruumble Client"
rm -rf "$W/win" && mkdir -p "$D"
cp "$WB/mumble.exe" "$D/ruumble-client.exe"
cp "$WB"/*.dll "$D/"
"$QT_ROOT_DIR/bin/windeployqt.exe" --release --no-translations --no-system-d3d-compiler --no-opengl-sw \
  "$(win "$D/ruumble-client.exe")" 2>&1 | tee "$W/deploy.log"
if [ -n "${VCToolsRedistDir:-}" ]; then
  cp "$(cygpath -u "$VCToolsRedistDir")"/x64/Microsoft.VC*.CRT/*.dll "$D/"
fi
install -m 644 "$RU/web/public/icon.svg" "$D/ruumble-client.svg"
cat > "$D/NOTICE.txt" <<'COPY'
Ruumble Client: Mumble (BSD-3-Clause, https://github.com/mumble-voip/mumble) with Ruumble (BSD-3-Clause) built in.
Uses Qt and Qt WebEngine (LGPL-3.0, Chromium), OpenSSL, Protocol Buffers, POCO, libsndfile, Opus, Boost, Speex and
RNNoise; their licences are those of the respective projects.
Contains the popover polyfill by OddBird (BSD-3-Clause, https://github.com/oddbird/popover-polyfill).
Start ruumble-client.exe.
COPY

mkdir -p "$ROOT/dist"
rm -f "$ROOT/dist/Ruumble_Client-x86_64-windows.zip"
(cd "$W/win" && 7z a -tzip "$(win "$ROOT/dist/Ruumble_Client-x86_64-windows.zip")" "Ruumble Client" >/dev/null)
echo "package: $ROOT/dist/Ruumble_Client-x86_64-windows.zip"
