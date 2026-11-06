#!/usr/bin/env bash
# Ticket #15: taking over an installed Mumble, headless against the local stack (test/local-stack.sh up).
# Builds a fake official Mumble setup (certificate of "Mia", a favourite, the accepted server certificate, an access
# token, a nickname) in isolated homes under .work/, starts Ruumble Client with RUUMBLE_MUMBLE_MIGRATION preset and
# checks the files and, through Ice, the connection: Mia is the registered user (same certificate) and may enter the
# token room (same token), and no certificate question stopped the connection (accepted server certificate).
#   migration.sh            all cases
# Never touches your real settings.
set -eu
HERE="$(cd "$(dirname "$0")" && pwd)"; W="$HERE/.work"; mkdir -p "$W"
ROOT="$(cd "$HERE/.." && pwd)"
BIN="$ROOT/work/build/mumble"
TOKEN=ruumble-test-token
fail=0
ok()  { echo "  ok    $1"; }
bad() { echo "  FAIL  $1"; fail=1; }
# expect <what> <actual> <expected>
expect() { if [ "$2" = "$3" ]; then ok "$1"; else bad "$1 (got: $2, expected: $3)"; fi; }

curl -sf http://127.0.0.1:64080/api/version >/dev/null || { echo "local stack not running: test/local-stack.sh up"; exit 2; }

# Mia's certificate (as Mumble keeps it: PKCS#12 without password, base64 in the settings) and its hash
K="$W/migration-keys"; mkdir -p "$K"
if [ ! -f "$K/mia.p12" ]; then
  openssl req -x509 -newkey rsa:2048 -nodes -keyout "$K/mia.key" -out "$K/mia.crt" -days 30 -subj "/CN=Mia" 2>/dev/null
  openssl pkcs12 -export -inkey "$K/mia.key" -in "$K/mia.crt" -passout pass: -out "$K/mia.p12"
  openssl req -x509 -newkey rsa:2048 -nodes -keyout "$K/old.key" -out "$K/old.crt" -days 30 -subj "/CN=Old" 2>/dev/null
  openssl pkcs12 -export -inkey "$K/old.key" -in "$K/old.crt" -passout pass: -out "$K/old.p12"
fi
MIA_CERT=$(base64 -w0 "$K/mia.p12"); OLD_CERT=$(base64 -w0 "$K/old.p12")
MIA_HASH=$(openssl x509 -in "$K/mia.crt" -noout -fingerprint -sha1 | cut -d= -f2 | tr -d ':' | tr 'A-F' 'a-f')
# the test server's certificate: its digest (accepted certificates) and its public key's digest (tokens)
openssl s_client -connect 127.0.0.1:64738 </dev/null 2>/dev/null | openssl x509 > "$K/server.crt"
SERVER_DIGEST=$(openssl x509 -in "$K/server.crt" -noout -fingerprint -sha1 | cut -d= -f2 | tr -d ':' | tr 'A-F' 'a-f')
TOKEN_DIGEST=$(openssl x509 -in "$K/server.crt" -noout -pubkey | openssl pkey -pubin -outform DER | sha1sum | cut -d' ' -f1)

read -r USER_ID TOKEN_ROOM < <(node "$HERE/migration-ice.cjs" prepare "$MIA_HASH" "$TOKEN" |
  python3 -I -c 'import json,sys; d=json.load(sys.stdin); print(d["userId"], d["tokenRoom"])')

# a fresh home; $1 name; $2 "mumble" for an official Mumble setup, "legacy" for only a Mumble 1.4 one
home() {
  H="$W/home-migration-$1"; rm -rf "$H"; mkdir -p "$H"
  MC="$H/.config/Mumble/Mumble"; MD="$H/.local/share/Mumble/Mumble"
  RC="$H/.config/Ruumble/Ruumble Client"; RD="$H/.local/share/Ruumble/Ruumble Client"
  if [ "$2" = legacy ]; then
    mkdir -p "$H/.config/Mumble"; printf '[audio]\nvoicehold=50\n' > "$H/.config/Mumble/Mumble.conf"; return
  fi
  mkdir -p "$MC" "$MD"
  cat > "$MC/mumble_settings.json" <<JSON
{
  "settings_version": 1, "certificate": "$MIA_CERT",
  "audio": { "transmit_mode": "PTT", "mute": true, "vad_min": 0.3 },
  "network": { "auto_connect_to_last_server": false },
  "last_connection": { "username": "Mia", "server_name": "Mumble HQ" },
  "misc": { "audio_wizard_has_been_shown": true, "database_location": "" },
  "ui": { "theme_style": "Lite" },
  "update": { "check_for_updates": false, "check_for_plugin_updates": false }
}
JSON
  python3 -I - "$MD/mumble.sqlite" "$SERVER_DIGEST" "$TOKEN_DIGEST" "$TOKEN" <<'PY'
import sqlite3, sys
db = sqlite3.connect(sys.argv[1])
db.executescript("""
CREATE TABLE servers (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT, hostname TEXT, port INTEGER DEFAULT 64738, username TEXT, password TEXT, url TEXT);
CREATE TABLE cert (id INTEGER PRIMARY KEY AUTOINCREMENT, hostname TEXT, port INTEGER, digest TEXT);
CREATE UNIQUE INDEX cert_host_port ON cert(hostname, port);
CREATE TABLE tokens (id INTEGER PRIMARY KEY AUTOINCREMENT, digest BLOB, token TEXT);
CREATE TABLE nicknames (id INTEGER PRIMARY KEY AUTOINCREMENT, hash TEXT, nickname TEXT);
CREATE TABLE pingcache (id INTEGER PRIMARY KEY AUTOINCREMENT, hostname TEXT, port INTEGER, ping INTEGER);
""")
db.execute("INSERT INTO servers (name, hostname, port, username, password) VALUES ('Mumble HQ', '127.0.0.1', 64738, 'Mia', '')")
db.execute("INSERT INTO cert (hostname, port, digest) VALUES ('127.0.0.1', 64738, ?)", (sys.argv[2],))
db.execute("INSERT INTO tokens (digest, token) VALUES (?, ?)", (bytes.fromhex(sys.argv[3]), sys.argv[4]))
db.execute("INSERT INTO nicknames (hash, nickname) VALUES ('abc', 'Ben from sales')")
db.execute("INSERT INTO pingcache (hostname, port, ping) VALUES ('127.0.0.1', 64738, 5)")
db.commit()
PY
}

# Ruumble Client set up already: another certificate, a server of its own and the same server under another name
existing_ruumble() {
  mkdir -p "$RC" "$RD"
  cat > "$RC/mumble_settings.json" <<JSON
{ "settings_version": 1, "certificate": "$OLD_CERT", "mumble_has_quit_normally": true,
  "audio": { "mute": false, "vad_min": 0.9 }, "ui": { "theme_style": "Dark" },
  "update": { "check_for_updates": false, "check_for_plugin_updates": false } }
JSON
  python3 -I - "$RD/mumble.sqlite" <<'PY'
import sqlite3, sys
db = sqlite3.connect(sys.argv[1])
db.executescript("""
CREATE TABLE servers (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT, hostname TEXT, port INTEGER DEFAULT 64738, username TEXT, password TEXT, url TEXT);
CREATE TABLE cert (id INTEGER PRIMARY KEY AUTOINCREMENT, hostname TEXT, port INTEGER, digest TEXT);
CREATE UNIQUE INDEX cert_host_port ON cert(hostname, port);
""")
db.execute("INSERT INTO servers (name, hostname, port, username, password) VALUES ('Only Ruumble', 'ruumble.example', 64738, 'Old', '')")
db.execute("INSERT INTO servers (name, hostname, port, username, password) VALUES ('Old name', '127.0.0.1', 64738, 'Old', '')")
db.execute("INSERT INTO cert (hostname, port, digest) VALUES ('127.0.0.1', 64738, 'stale')")
db.commit()
PY
}

# starts Ruumble Client in $H with the answer $1, optionally with a link ($2), and stops it again after $3 seconds
start() {
  env -i HOME="$H" PATH=/usr/bin:/bin LANG=en_US.UTF-8 XDG_RUNTIME_DIR="$H/run" QT_QPA_PLATFORM=offscreen \
    QTWEBENGINE_CHROMIUM_FLAGS="--disable-gpu" RUUMBLE_LOG_STDERR=1 RUUMBLE_MUMBLE_MIGRATION="$1" \
    bash -c 'mkdir -p "$XDG_RUNTIME_DIR" && chmod 700 "$XDG_RUNTIME_DIR" && exec "$0" "$@"' \
    "$BIN" ${2:+"$2"} > "$H/client.log" 2>&1 &
  CLIENT=$!
}
stop() { kill "$CLIENT" 2>/dev/null || true; wait "$CLIENT" 2>/dev/null || true; }

json() { python3 -I -c "import json,sys; d=json.load(sys.stdin if sys.argv[1]=='-' else open(sys.argv[1])); print(eval(sys.argv[2]))" "$@"; }
sql()  { python3 -I -c "import sqlite3,sys; print(sqlite3.connect(sys.argv[1]).execute(sys.argv[2]).fetchall())" "$@"; }

connects_as_mia() {
  start take "mumble://Mia@127.0.0.1:64738/"
  local seen; seen=$(node "$HERE/migration-ice.cjs" check Mia "$TOKEN_ROOM")
  stop
  expect "connects without a certificate question" "$(json - 'd["session"] is not None' <<<"$seen")" True
  expect "is the registered user (Mumble's certificate)" "$(json - 'd.get("userId")' <<<"$seen")" "$USER_ID"
  expect "may enter the token room (Mumble's token)" "$(json - 'd.get("mayEnter")' <<<"$seen")" True
}

echo "== fresh Ruumble Client, take over"
home fresh mumble
connects_as_mia
S="$RC/mumble_settings.json"; DB="$RD/mumble.sqlite"
expect "answer remembered" "$(grep -c 'taken over' "$RC/mumble-migration")" 1
expect "certificate taken over" "$(json "$S" 'd["certificate"]')" "$MIA_CERT"
expect "audio taken over without the mute state" "$(json "$S" '(round(d["audio"]["vad_min"], 2), d["audio"].get("mute", False))')" "(0.3, False)"
expect "user name taken over" "$(json "$S" 'd["last_connection"]["username"]')" Mia
expect "look not taken over" "$(json "$S" 'd.get("ui", {}).get("theme_style") == "Lite"')" False
expect "favourite taken over" "$(sql "$DB" "select name, username from servers")" "[('Mumble HQ', 'Mia')]"
expect "nickname taken over" "$(sql "$DB" "select nickname from nicknames")" "[('Ben from sales',)]"

echo "== Ruumble Client set up already, overwrite"
home existing mumble; existing_ruumble
connects_as_mia
S="$RC/mumble_settings.json"; DB="$RD/mumble.sqlite"
expect "certificate replaced by Mumble's" "$(json "$S" 'd["certificate"]')" "$MIA_CERT"
expect "own settings outside the taken-over parts stay" "$(json "$S" 'd["ui"]["theme_style"]')" Dark
expect "backup of the settings holds the old certificate" "$(grep -l "$OLD_CERT" "$RC"/mumble_settings.json.before-mumble-migration-* | wc -l)" 1
expect "backup of the database" "$(ls "$RD"/mumble.sqlite.before-mumble-migration-* | wc -l)" 1
expect "server only in Ruumble Client stays, Mumble's row wins" "$(sql "$DB" "select name, username from servers order by name")" "[('Mumble HQ', 'Mia'), ('Only Ruumble', 'Old')]"
expect "accepted server certificate is Mumble's" "$(sql "$DB" "select digest from cert")" "[('$SERVER_DIGEST',)]"
expect "caches not taken over" "$(sql "$DB" "select count(*) from pingcache")" "[(0,)]"

echo "== not now"
home notnow mumble; existing_ruumble
start not-now; sleep 4; stop
expect "nothing remembered, so the next start asks again" "$(ls "$RC/mumble-migration" 2>/dev/null | wc -l)" 0
expect "Ruumble Client unchanged" "$(json "$RC/mumble_settings.json" 'd["certificate"]')" "$OLD_CERT"

echo "== do not ask again"
home never mumble; existing_ruumble
start never; sleep 4; stop
expect "answer remembered" "$(grep -c 'do not ask again' "$RC/mumble-migration")" 1
expect "Ruumble Client unchanged" "$(json "$RC/mumble_settings.json" 'd["certificate"]')" "$OLD_CERT"
start take; sleep 4; stop
expect "a later start does not take over" "$(json "$RC/mumble_settings.json" 'd["certificate"]')" "$OLD_CERT"

echo "== only an older Mumble (1.4)"
home legacy legacy
start take; sleep 4; stop
expect "nothing taken over" "$(ls "$H/.config/Ruumble/Ruumble Client/mumble-migration" 2>/dev/null | wc -l)" 0
expect "the hint is logged" "$(grep -c 'older Mumble setup' "$H/client.log")" 1

[ $fail = 0 ] && echo "all migration checks passed" || { echo "some migration checks failed (logs: $W/home-migration-*/client.log)"; exit 1; }
