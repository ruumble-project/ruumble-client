# Roadmap: Ruumble Desk (Mumble fork with Ruumble built in)

Local pilot of ADR-0024 (`docs/ADR-0024-draft.md`). Each milestone ends with something that runs and
can be checked. Status: `[ ]` open, `[~]` in progress, `[x]` done.

## M0: Base build

- [x] Own repo, branch `ruumble/main` on `v1.6.870`, submodules
- [x] Build dependencies installed (Qt 6.4.2 incl. WebEngine)
- [x] Configure of the client-only build
- [x] Unchanged client builds and starts
- [x] Connects to the local Mumble server from `Ruumble/deploy/local`

Check: the plain client works, voice included.

## M1: Own identity (D5, D6)

- [x] Own application name "Ruumble Desk", so settings, plugin dir and data dir are separate
      from the official Mumble 1.5 install
- [x] External Ruumble plugin is ignored by the fork (no double connect)
- [x] Certificate: taken over automatically on first start from `~/Documents/MumbleAutomaticCertificateBackup.p12`
      (Mumble's own backup, exists on this machine); manual .p12 import only if that backup is outdated

Check: official client and fork run side by side without touching each other's settings.

## M2: Phase 0, built-in core

- [x] CMake option `ruumble`: compiles Ruumble's core sources from `RUUMBLE_SOURCE_DIR`,
      IXWebSocket via FetchContent, no change to the Ruumble repo
- [x] `QtMumbleApi : MumbleApi` mirrors `Ruumble/plugin/src/plugin.cpp`, every call runs on the
      main thread (like `API_v_1_x_x.cpp`), the worker waits at most 800 ms
- [~] Core lifecycle tied to server connect/disconnect, clean stop before `MainWindow` teardown
      (connect, Mumble server restart with automatic reconnect, room move afterwards, SIGTERM tested; quitting through the menu still to test by hand)
- [x] Own paired state file (not the plugin's `plugin.json`)
- [x] ~~Browser still opens via `OpenUrl`~~ dropped: M2 and M3 were built together, the link opens in the view

Check: the bridge sees the fork as a plugin, pairing in the browser works, room click moves
you, mute/deaf and talking work, quitting never hangs.
Result 2026-11-03 (headless, `test/`): plugin connected, pairing, room move, mute, deaf ok.
Open: talking (no microphone in the headless test), quit through the menu.

## M3: Phase 1, one window

- [x] `RuumbleView` (QWebEngineView) as central view, Mumble's tree and log as docks or toggle
- [x] `OpenUrl(pairUrl)` loads in the view: silent pairing on first connect
- [x] Persistent `QWebEngineProfile` (cookie survives restart, `ForcePersistentCookies` if needed)
- [x] `AA_ShareOpenGLContexts` set before `QApplication`
- [x] Sensible state while not connected or no service found (placeholder page)

Check: the "done" criteria in `../CLAUDE.md`, end to end against the local stack, restart keeps
the pairing.
Result 2026-11-03 (headless): all met, restart keeps the pairing. Open: one visible run on the
real desktop (layout of view and user dock, first-start audio wizard).

## M3.5: Manual check on the desktop (Kevin)

- [ ] `test/local-stack.sh up`, then `tools/run.sh` (no address: the server manager opens like in Mumble) and connect to `127.0.0.1:64738`
- [ ] Layout ok (building in the middle, Users dock on the right, log/chat docks)
- [ ] Talking indicator in the building while speaking
- [ ] Quit through the menu: no hang
- [ ] Next to the official client with the Ruumble plugin: only one connects to the service
      (same certificate on the same server: the Mumble server may kick the older connection)

## M3.6: Ruumble UI as host (Kevin's choice 2026-11-03)

- [x] Mumble's menu bar, toolbar and docks (log, chat, user list) hidden; the building fills the window
      (`RuumbleIntegration::applyChrome`, called at the end of `MainWindow::setupView`)
- [x] Window title from the page ("Lobby · Ruumble"), window icon from the favicon
- [x] `DeskChannel`: object "desk" over QWebChannel (log as plain text, channel chat, dialogs), only for the
      service's own page; `qwebchannel.js` injected into the page
- [x] `Log::messageLogged` signal (one touch point in `Log.{h,cpp}`)
- [x] Web UI (Ruumble repo): `web/src/lib/desk/`, `web/src/lib/ui/desk/DeskPanel.svelte`, button in the top bar,
      i18n `desk`, tests `web/test/desk.test.ts`, CHANGELOG line. Only visible inside Ruumble Desk.
- [x] Placeholder in Ruumble style with "Connect to a server", "Settings", "Audio wizard" (no Mumble menus left)
- [x] Headless: panel, log, chat both ways (Anna to Ben), unread count, disconnect, placeholder
- [ ] Visible check by Kevin: no Mumble chrome left, dialogs open over the building
- [x] All Mumble functions stay reachable (Kevin): "Mumble menu" (panel header, user menu, F10) pops up the menu
      bar's own QMenus natively (Server with server manager, ban list, information, tokens; Self; Configure; Help;
      Developer when enabled), so Mumble's aboutToShow logic and future entries keep working
- [x] Mumble user list as an optional dock (last entry of the Mumble menu): channel and user administration
      (ACL, edit, add channel, kick, ban, register) through its context menu
- [x] Mumble settings easy to reach (Kevin): user menu entry "Mumble settings", gear in the Mumble panel,
      Ctrl+, everywhere (also on the placeholder); the dialog itself stays Mumble's
- [x] Crash on start with a window state saved by an earlier build (Qt layout): Ruumble Desk no longer restores
      Mumble's dock arrangement, only the window geometry; the user tree is taken out with `takeCentralWidget`


- [x] The core logs "Hover once over the top channel ..." on the first pending description,
      although Ruumble Desk fetches the text itself a moment later. Fix 2026-11-04: core setting `hoverHint`
      (default on for the plugin), Desk turns it off; doctest for both
- [x] `?embedded` handling in the web UI (2026-11-04): no URL flag needed, the web UI knows Desk by its `desk` object.
      Inside Desk the "not paired" and "Mumble offline" screens drop the plugin download and install steps, say that
      Desk pairs itself, and offline offer "Connect to a server" (`desk.openConnect`). Tests: `web/e2e/desk.spec.ts`
      (fake QWebChannel), i18n in `web/test/desk.test.ts`
- [ ] Desktop entry and icon for the local build
- [x] Touch points listed as a patch overview (basis for a later R2 patch stack): `docs/touch-points.md` (2026-11-04)

## M6: Program and AppImage (Kevin, 2026-11-03)

- [x] `./build.sh`: prepare, build, `dist/ruumble-desk` with icon and desktop entry
- [x] `./build.sh --install`: start menu entry "Ruumble Desk" and command `ruumble-desk`; `--uninstall` removes them
- [x] Own desktop ID `ruumble-desk` (patch in `main.cpp`), not grouped with the official Mumble in the taskbar
- [x] `tools/appimage.sh` (`./build.sh --appimage`): linuxdeploy + Qt plugin, pinned versions, SQLite driver and
      OpenSSL TLS backend added on top of what the plugin finds
- [ ] First AppImage run: Kevin starts it (downloading and running linuxdeploy was blocked for Claude by the
      permission settings); then check that it starts, connects and shows the building, and whether Qt WebEngine's
      sandbox works inside the AppImage (fallback: `QTWEBENGINE_DISABLE_SANDBOX=1` in an AppRun hook)

## M7: Packages for any Linux (Kevin, 2026-11-03)

- [x] `./package.sh`: builds in a container (only Docker on the host), Mumble and Ruumble mounted read only
- [x] Native package per distro (`--distro debian12|ubuntu2404`): .deb (dpkg-shlibdeps) plus the run-time parts;
      Release, stripped. Fedora .rpm and Arch package removed 2026-11-04 (Kevin: AppImage and .deb are enough; Fedora's
      OpenSSL 4 breaks Mumble 1.6's build, upstream fix `1f00fb39b8` is only on master)
- [x] AppImage from Debian 12 (glibc 2.36) for the widest reach
- [x] `.deb` for Debian 12 built and checked (2026-11-04, 5 min): `ruumble-desk_0.33.0+debian12_amd64.deb`, 16 MB,
      only `/usr/bin/ruumble-desk`, desktop entry, icon and copyright; dependencies from dpkg-shlibdeps (Qt 6.4
      WebEngine, WebChannel, libssl3, opus, poco, protobuf, ...)
- [~] `.deb` for Ubuntu 24.04 built 2026-11-04 (8 min, same contents, `t64` library names); still open: Kevin
      installs it here (`sudo apt install ./desk/dist/ruumble-desk_0.33.0+ubuntu2404_amd64.deb`)
- [~] AppImage built 2026-11-04 with `./package.sh appimage` (Debian 12 container, linuxdeploy download worked there):
      `dist/Ruumble_Desk-x86_64.AppImage`, 131 MB, built against glibc 2.36 (Debian 12); contains QtWebEngineProcess, WebEngine resources,
      SQLite driver and OpenSSL TLS backend, no unresolved libraries. Fix on the way: `tools/appimage.sh` had no
      executable bit. Still open: Kevin starts it on the desktop (WebEngine sandbox inside the AppImage)
- [ ] Windows and macOS: not covered (other toolchains: MSVC/vcpkg, macOS bundle); separate decision

## M8: Ruumble moves into this repo (Kevin, 2026-11-04, agreed with Bert)

Goal: nothing depends on `Skrrytch/Ruumble` any more, neither the build nor the server image; that repo could be
deleted. Done on the branch `merge-ruumble`, pushed to `main` on 2026-11-04 (Kevin).

- [x] Desk moved into `desk/`, Ruumble merged at the repo root with its full history (`3cc31c2` = v0.32.1, 235
      commits, without Bert's tags so no old release runs again); LICENSE names Bert Speckels and Kevin Messerschmidt
- [x] Root `.gitattributes` keeps LF everywhere (`core.autocrlf=true` here checked Ruumble out with CRLF)
- [x] Desk panel is a normal commit in `web/`, `patches/ruumble` removed
- [x] Desk build uses the repo root as Ruumble; Mumble from GitHub or a checkout in `MUMBLE_REPO` (`pins.env`
      `MUMBLE_URL`), also in the packaging container; version follows `bridge/package.json`, `packaging/VERSION` removed
- [x] References to `Skrrytch/Ruumble` replaced: image `ghcr.io/kevinwebman/ruumble`, demo
      compose files (GitHub Pages demo dropped, Kevin 2026-11-04), docs, `tools/release.sh`, release check; history (old
      changelog links, ADR-0009, charter) left as it was, ADR-0008 got a "current state" line
- [x] `release.yml`: job `appimage` (desk/package.sh appimage), attached as `Ruumble_Desk-<version>-x86_64.AppImage`
- [x] Checks 2026-11-04: root `pnpm lint && pnpm test && pnpm build` green (409 tests), `docker build -f
      deploy/Dockerfile .` ok, `./build.sh` ok (core 0.5.0 from the repo root), headless run connects, pairs and
      shows the Desk panel button, `MUMBLE_REPO=/nonexistent desk/package.sh appimage` ok (Mumble cloned from
      GitHub as in CI, 5.5 min). Web e2e (Playwright) 106 passed. Not yet run on GitHub Actions: a manual dry run of `release.yml` would check that
- [x] Pushed `merge-ruumble` to `main` (2026-11-04); Pages dropped (Kevin)
- [x] Kevin: keep issues on (release watch opens them), after the first release make the GHCR package `ruumble` public
      (2026-11-04: anonymous pull of `ghcr.io/kevinwebman/ruumble:0.33.0` works)
- [x] First release from this repo, v0.33.0 (2026-11-04): image `ghcr.io/kevinwebman/ruumble:0.33.0` (also `0.33`,
      `latest`), plugin, `Ruumble_Desk-0.33.0-x86_64.AppImage`. The AppImage job ran on Actions for the first time and
      passed; "Create the GitHub release" failed once on GitHub errors (asset uploaded twice, then HTTP 502), "Re-run
      failed jobs" fixed it without a new tag
- [~] GHCR package `ruumble` public (done); the colleague switches his image line to `ghcr.io/kevinwebman/ruumble`
- [x] Mumble updates (Kevin, 2026-11-04: no API key on GitHub, run by hand here): `desk/tools/update-mumble.sh [tag]`
      (interface files, pin, full clone, 3-way merge of the patch, build, export) and the Claude Code skill
      `.claude/skills/update-all` ("Update alles": runs it, fixes conflicts, all checks, report). The weekly
      `mumble-release-watch.yml` only reports a new release as an issue and starts the live tests, as before
- [x] Working mode like Bert's (Kevin, 2026-11-04): commit and push directly to `main`, Bert joins as collaborator;
      `Skrrytch/Ruumble` will probably be deleted once this repo runs on its own

## Bugs

- [x] 2026-11-04 (Kevin): things showing up at the right edge, board not working right. Cause: Desk's engine is
      Chromium 102 (Qt 6.4), the web UI used the popover API (Chrome 114), `color-mix()` (111) and container units
      (105). Fixed in the web UI: popover polyfill loaded only where missing, fallbacks, build target `chrome102`.
      Checked headless in Desk: person card placed over the avatar, room description opens, closes on a click beside
      it (trusted click, `cdp.mjs click`), board light and posting works; web e2e unchanged.
      Follow-up the same day: Kevin still saw the bugs on the colleague's server (old 0.32.1 web UI). Desk now injects
      the polyfill and CSS fallbacks itself where the engine lacks them (`client/compat/`, `RuumbleView::compatScript`),
      so it works against any Ruumble service. Checked against the 0.32.1 web UI built from `3cc31c2`. Lesson: a
      declaration with `var()` is only dropped when computed, so a plain declaration before it is no fallback;
      `@supports` is (the web fix uses it now too); the board is an `aside`
- [x] 2026-11-04 (Kevin): no sound, could not talk in Desk. Cause: Desk has its own settings (by design), so Kevin's
      push-to-talk shortcuts and audio settings from the official Mumble were missing. Fix: on the first start
      `RuumbleIntegration::importMumbleSettings()` (called in `main.cpp` before the settings load) takes the official
      client's `audio` section (without its mute/deaf state) and its shortcuts once; backup
      `mumble_settings.json.before-mumble-import`, marker `mumble-settings-imported` in `~/.config/Ruumble/Ruumble Desk/`.
      Existing Desk installs get it on their next start. Checked in an isolated home
- [x] 2026-11-04 (Kevin): no Mumble chat on the colleague's server. Cause: log and chat live in the Desk panel of
      the web UI, which an older service (0.32.1) does not have, and Desk hides Mumble's own docks. Fix: when the
      service's page has not asked `desk.history()` 4 s after loading, Desk shows Mumble's log and chat docks; the
      Mumble menu has "Mumble log and chat" to switch them by hand. Checked against the 0.32.1 and the current web UI
- [x] 2026-11-04 (Kevin): everything Mumble can do, in the building. Right click on room, corridor, floor sign,
      elevator floor or person opens Mumble's own `qmChannel`/`qmUser` (`RuumbleIntegration::showChannelMenu`,
      `showUserMenu`: the target is selected in the hidden user tree first, because Mumble takes that before
      `cContextChannel`, and the old selection is restored), the building line in the elevator opens the root
      channel's menu (new floor). Desk adds "Move to floor" to the channel menu. People are dragged onto rooms
      (`desk.moveUser`, follows Mumble's "user dragging" setting). Destructive actions only through the native menu
      (ADR-0017). Web: `web/src/lib/desk/admin.ts` (context), active only inside Desk. Settings entries moved from
      the user menu into the Desk panel (Kevin). Checked headless with trusted right clicks (menus log their
      entries) and drag events; a real mouse drag cannot run in Qt's offscreen platform (dragstart, then dragend at
      once), so Kevin checks the drag on the desktop. Test data: `desk/test/seed.cjs` (descriptions, everyone may
      administer the LOCAL test building)
- [x] 2026-11-04: Ruumble Desk used the official Mumble's D-Bus name `net.sourceforge.mumble.mumble` for its
      single-instance check. Starting Desk with a `mumble://` URL while the official Mumble ran sent the URL to the
      official client, which left Kevin's work session for the local test server (about 11 s). Fix in
      `patches/mumble` (`main.cpp`): Desk calls and registers `net.sourceforge.mumble.ruumbledesk`. Checked: the
      session bus lists the Desk name, Desk starts as its own process
- [x] 2026-11-04: after the repo moved (`Ruumumble/ruumble-desk` to `ruumble-desk`) `./build.sh` failed: `work/build`
      was configured at the old path and the `client/` link in `work/mumble` pointed there too. Fix: `tools/prepare.sh`
      refreshes the link every time, `tools/build.sh` runs it and configures `work/build` again when its cache names
      another source dir
- [ ] Kevin: drag a person with the mouse on the desktop
- [x] Container queries in room labels (`SpaceButton.svelte`, compact labels in narrow rooms) are ignored in
      Chromium 102. Fix 2026-11-04: the room's measured width sets the class `narrow`, used only under
      `@supports not (container-type: inline-size)`; the 170 px rule was dead (rooms are at least 190 px) and is gone.
      Checked headless in Desk: a room forced to 200 px shows the count behind the name, at 300 px below it

## Later, outside this pilot

- Phase 2: immediate refusal signal from `msgPermissionDenied` (upstream candidate)
- Update routine onto new Mumble tags (rebase, `git rerere`)
- Packaging (AppImage, Windows installer), CI
- Screen sharing (SFU; needs Qt newer than 6.4 for the screen picker)
- Formal decision on ADR-0024 D1 to D10

## M5: Own repo that only uses Mumble and Ruumble (Kevin, 2026-11-03)

- [x] `ruumble-desk` with `client/`, `patches/mumble`, `patches/ruumble`, `pins.env`, `tools/`, `test/`, docs
- [x] `tools/prepare.sh` clones `../mumble` and `../Ruumble` read only into `work/` and applies the patches
- [x] Ruumble repo back to its original state (`3cc31c2`, no changes); `../mumble` never touched
- [~] Build from `work/` and headless check
- [x] Old `ruumble-mumble-fork` removed (2026-11-04, together with the local `mumble` and `Ruumble` clones; checked first: nothing in them that is not in `desk/`)
- Since M8 the separate `../Ruumble` checkout and `patches/ruumble` are gone; Ruumble is the repo root.

## Notes

- Paths below written before M5 refer to the old `ruumble-mumble-fork`: `src/mumble/ruumble/` is now `client/`,
  `ruumble/test/` is `test/`, Mumble file changes are in `patches/mumble`, web changes in `patches/ruumble`.

- 2026-11-03: M0 base build ok. M2 and M3 code written and compiling (`src/mumble/ruumble/`,
  touch points under `#ifdef USE_RUUMBLE` in `main.cpp`, `MainWindow.{h,cpp}`, `PluginManager.cpp`,
  block at the end of `src/mumble/CMakeLists.txt`). `[x]` on code items means "implemented and
  builds"; the check lines of M2/M3 are only met after the end-to-end run.
- Data: `~/.local/share/Ruumble/Ruumble Desk/` (paired state `ruumble.json`, web profile
  `ruumble-web/`), settings `~/.config/Ruumble/Ruumble Desk/mumble_settings.json`.
- Single-instance socket is "RuumbleDesk", so the fork runs next to the official client
  without `--multiple`.
- Root description: unlike the plugin, the fork requests the blob itself, so nobody has to
  click on the root channel (ADR-0010 hint should no longer appear).
- Local test setup: `test/README.md`. The service runs on the host (the Docker image build is
  very slow here), with `ADDRESS_CHECK=warn` because Mumble in Docker sees the host as 172.x.
- Found in passing (Ruumble repo, not touched): `pnpm -F @ruumble/protocol build` fails,
  `schema/protocol.schema.json` is out of date on `main` (Node 24 here; maybe Node 22 differs). Checked again 2026-11-04 with Node 24: up to date.
- Roughly 50 changed lines in Mumble files (all under `#ifdef USE_RUUMBLE`) plus the CMake block;
  `patches/mumble` holds the touch points.
- After every `web` build the Ruumble service must be restarted (`test/local-stack.sh up`): it only serves the
  asset files that existed when it started, new hashed files get 404 and the page stays empty.
- Tray icon, global shortcuts and Mumble's own dialogs (settings, connect, certificate) stay Mumble's.
