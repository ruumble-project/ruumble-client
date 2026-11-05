#!/usr/bin/env bash
# Starts Ruumble Client headless (offscreen) in an isolated HOME against the local stack.
#   run-desk.sh <Name> [fresh]   fresh = wipe the test home first
#   DESK_LINK=0 starts without a mumble:// link: the City Map (or "On your next visit") decides what happens
#   DESK_AUTOCONNECT=1 sets Mumble's "connect automatically" (with the City Map's city-map.json: "On your next visit"),
#   DESK_LAST_SERVER=<name> the last visited building, DESK_LANG=de_DE the system language
# WebEngine DevTools on 127.0.0.1:9333 (for Playwright over CDP). Log: $T/desk-<Name>.log
set -eu
T="$(cd "$(dirname "$0")" && pwd)/.work"; mkdir -p "$T"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
user=$1
H="$T/home-$user"
[ "${2:-}" = fresh ] && rm -rf "$H"
conf="$H/.config/Ruumble/Ruumble Client"; data="$H/.local/share/Ruumble/Ruumble Client"
mkdir -p "$conf" "$data" "$H/Documents"

if [ ! -f "$H/Documents/MumbleAutomaticCertificateBackup.p12" ]; then
  openssl req -x509 -newkey rsa:2048 -nodes -keyout "$H/c.key" -out "$H/c.crt" -days 30 -subj "/CN=$user" 2>/dev/null
  openssl pkcs12 -export -inkey "$H/c.key" -in "$H/c.crt" -passout pass: -out "$H/Documents/MumbleAutomaticCertificateBackup.p12"
fi
# push-to-talk without a key: never sends the microphone
autoconnect=false; [ "${DESK_AUTOCONNECT:-0}" = 1 ] && autoconnect=true
cat > "$conf/mumble_settings.json" <<JSON
{
  "settings_version": 1,
  "mumble_has_quit_normally": true,
  "misc": { "audio_wizard_has_been_shown": true },
  "audio": { "transmit_mode": "PTT", "mute_cue_popup_shown": true },
  "network": { "auto_connect_to_last_server": $autoconnect },
  "last_connection": { "server_name": "${DESK_LAST_SERVER:-}" },
  "update": { "check_for_updates": false, "check_for_plugin_updates": false }
}
JSON
# fixed service address for the test (discovery is tested separately via the root description)
if [ -n "${DESK_BRIDGE_URL:-}" ]; then
  printf '{ "bridgeUrl": "%s", "autoOpen": true }\n' "$DESK_BRIDGE_URL" > "$data/ruumble.json"
fi
digest=$(echo | openssl s_client -connect 127.0.0.1:64738 2>/dev/null | openssl x509 -noout -fingerprint -sha1 | cut -d= -f2 | tr -d ':' | tr 'A-F' 'a-f')
python3 -I - "$data/mumble.sqlite" "$digest" <<'PY'
import sqlite3, sys
db = sqlite3.connect(sys.argv[1])
db.execute("CREATE TABLE IF NOT EXISTS cert (id INTEGER PRIMARY KEY AUTOINCREMENT, hostname TEXT, port INTEGER, digest TEXT)")
db.execute("DELETE FROM cert WHERE hostname='127.0.0.1' AND port=64738")
db.execute("INSERT INTO cert (hostname, port, digest) VALUES ('127.0.0.1', 64738, ?)", (sys.argv[2],))
db.commit()
PY

link="mumble://$user@127.0.0.1:64738/"
[ "${DESK_LINK:-1}" = 0 ] && link=""
env -i HOME="$H" PATH=/usr/bin:/bin LANG=${DESK_LANG:-en_US}.UTF-8 XDG_RUNTIME_DIR="$H/run" \
  QT_QPA_PLATFORM=offscreen QTWEBENGINE_REMOTE_DEBUGGING=127.0.0.1:${DEVTOOLS_PORT:-9333} \
  QTWEBENGINE_CHROMIUM_FLAGS="--disable-gpu" RUUMBLE_LOG_STDERR=1 \
  bash -c 'mkdir -p "$XDG_RUNTIME_DIR" && chmod 700 "$XDG_RUNTIME_DIR" && exec "$0" "$@"' \
  "$ROOT/work/build/mumble" ${link:+"$link"} > "$T/desk-$user.log" 2>&1 &
echo $! > "$T/desk-$user.pid"
echo "started pid $(cat "$T/desk-$user.pid"), log $T/desk-$user.log"
