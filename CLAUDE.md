# CLAUDE.md: ruumble-desk

Ruumble Desk is a Mumble client with Ruumble built in (local pilot of ADR-0024, draft in `docs/ADR-0024-draft.md`).
Read `README.md` for the layout and the tools, `ROADMAP.md` for the state.

## The one rule

**The original repos are never changed.** `../mumble` and `../Ruumble` are only read: `tools/prepare.sh` clones
them into `work/` and applies `patches/`. All work happens in this repo (`client/`, `patches/`, `tools/`, `test/`,
docs). Never commit, edit, `pnpm install` or build inside `../mumble` or `../Ruumble`; use `work/` instead.

Kevin's rules: no commits and no pushes unless he asks (then one-line messages, no bullets, no `Co-Authored-By`);
no em or en dashes anywhere; code, comments and docs in English.

Progress lives in `ROADMAP.md`: tick off every finished item right away (`[x]`, `[~]` in progress) and add notes,
so Kevin or the next session can continue at any point.

## Scope

Works well **on this machine**: phases 0 and 1 of the ADR plus "Ruumble UI as host" (Kevin's choice):

- Ruumble's plugin core (`work/Ruumble/plugin/src/{core,config,discovery,messages,net}.cpp`, unchanged) is compiled
  into the client; `client/QtMumbleApi` implements its `MumbleApi` on the client's own classes.
- `client/RuumbleView`: the Ruumble web UI from the service is the whole window (QWebEngineView, persistent profile,
  pairing link opens in the view).
- `client/RuumbleIntegration`: hides Mumble's menu bar, toolbar and docks; Mumble's menus stay reachable as a native
  popup (F10, panel, user menu), the user list as an optional dock, settings with Ctrl+,.
- `client/DeskChannel`: object `desk` over QWebChannel (contract in `DeskChannel.h`), used by the web part in
  `patches/ruumble` (`web/src/lib/desk/`, `web/src/lib/ui/desk/DeskPanel.svelte`). Keep both sides in sync.

Working assumptions from the ADR (the owner said "just do it"; the ADR stays a draft): own settings dir and name
"Ruumble Desk", UI loaded from the service URL, Mumble 1.6 only. Not in scope: phase 2, screen sharing (needs Qt
newer than 6.4), packaging, Windows, CI.

## Code rules

- Mumble touch points stay minimal and under `#ifdef USE_RUUMBLE` (`patches/mumble`); everything else in `client/`.
- The core calls `MumbleApi` from its worker thread: every call runs on the main thread with an 800 ms timeout
  (`QtMumbleApi::onMain`), so shutdown cannot hang.
- The `desk` object only answers the service's own page (`RuumbleView::showsService`); log texts go to the page as
  plain text, never Mumble's HTML.

## Build dependencies (Mint 22.3 / Ubuntu 24.04, installed once)

```sh
sudo apt install build-essential cmake ninja-build pkgconf qt6-base-dev qt6-tools-dev \
  qt6-tools-dev-tools qt6-svg-dev qt6-l10n-tools qt6-webengine-dev qt6-webengine-dev-tools \
  qt6-webchannel-dev libgl-dev libboost-dev libssl-dev libprotobuf-dev protobuf-compiler \
  libprotoc-dev libcap-dev libxi-dev libasound2-dev libpulse-dev libogg-dev libsndfile1-dev \
  libopus-dev libpoco-dev nlohmann-json3-dev libcli11-dev libutfcpp-dev libspdlog-dev libspeexdsp-dev
```

## Gotchas

- After a web change in `work/Ruumble`: `test/local-stack.sh up` again; the service only serves the asset files it
  saw at start (new hashed files get 404, the page stays empty).
- Web checks in the work tree: `cd work/Ruumble/web && corepack pnpm lint && corepack pnpm test`.
- Mumble in Docker sees the host as the Docker gateway (172.x): the local service runs with `ADDRESS_CHECK=warn`.
- Headless tests (`test/run-desk.sh`) use an isolated HOME; Playwright cannot attach to QtWebEngine, `test/cdp.mjs`
  speaks raw CDP. Rooms are entered by double-click.
