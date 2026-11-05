#!/usr/bin/env bash
# Local test stack for Ruumble Client: Mumble server in Docker (deploy/local), Ruumble service on the host,
# both from the server submodule (server/).
#   local-stack.sh up | down
# The service runs on the host because the Docker image build is very slow here (npm in the container).
# ADDRESS_CHECK=warn: Mumble in Docker sees the host as the Docker gateway (172.x), the service sees 127.0.0.1.
set -eu
HERE="$(cd "$(dirname "$0")" && pwd)"; W="$HERE/.work"; mkdir -p "$W/bridge-data"
RUUMBLE="$(cd "$HERE/../server" && pwd)"
export COREPACK_ENABLE_DOWNLOAD_PROMPT=0
case "${1:-up}" in
  up)
    docker compose -f "$RUUMBLE/deploy/local/docker-compose.yml" up -d mumble
    (cd "$RUUMBLE" && corepack pnpm install >/dev/null && corepack pnpm -F @ruumble/web build >/dev/null && corepack pnpm -F @ruumble/bridge build >/dev/null)
    (cd "$RUUMBLE/tools/live-test" && corepack pnpm install >/dev/null && corepack pnpm gen >/dev/null)
    sleep 3
    # long root description: Mumble then only sends a hash, Ruumble Client has to fetch the text itself (ADR-0010)
    (cd "$RUUMBLE/tools/live-test" && RUUMBLE_URL=http://127.0.0.1:64080 RUUMBLE_DESC_PAD=1 node src/setup.cjs >/dev/null)
    pkill -f "node dist/main.mjs" 2>/dev/null || true
    # setsid + all three streams redirected: the service must not hold the caller's terminal or pipe
    (cd "$RUUMBLE/bridge" && ICE_HOST=127.0.0.1 ICE_SECRET_READ=s1-read-secret WEB_DIST=../web/dist DATA_DIR="$W/bridge-data" \
      PORT=64080 HOST=127.0.0.1 ADDRESS_CHECK=warn setsid -f node dist/main.mjs < /dev/null > "$W/bridge.log" 2>&1)
    sleep 3; curl -s http://127.0.0.1:64080/api/version; echo; echo "service log: $W/bridge.log"
    ;;
  down)
    pkill -f "node dist/main.mjs" 2>/dev/null || true
    docker compose -f "$RUUMBLE/deploy/local/docker-compose.yml" down
    ;;
esac
