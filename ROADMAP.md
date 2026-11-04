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


- [ ] The core logs "Hover once over the top channel ..." on the first pending description,
      although Ruumble Desk fetches the text itself a moment later (cosmetic, needs a core option in Ruumble)
- [ ] `?embedded` handling in the web UI (needs a change in the Ruumble repo, owner decision)
- [ ] Desktop entry and icon for the local build
- [ ] Touch points listed as a patch overview (basis for a later R2 patch stack)

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
- [x] Native package per distro (`--distro debian12|ubuntu2404|fedora|arch`): .deb (dpkg-shlibdeps), .rpm
      (rpmbuild requires), .pkg.tar.zst (makepkg, deps via `pacman -Qo`), plus the run-time parts; Release, stripped
- [x] AppImage from Debian 12 (glibc 2.36) for the widest reach
- [ ] `.deb` for Debian 12 built and checked (contents, dependencies). 2026-11-03: first container run started and
      stopped on purpose during the build (continue next day); nothing of M7 has produced a package yet
- [ ] `.deb` for Ubuntu 24.04 built and installed here (`sudo apt install ./dist/...`, Kevin)
- [ ] Fedora .rpm and Arch package built (newer Qt and GCC than Debian: may need small fixes)
- [~] AppImage built 2026-11-04 with `./package.sh appimage` (Debian 12 container, linuxdeploy download worked there):
      `dist/Ruumble_Desk-x86_64.AppImage`, 131 MB, built against glibc 2.36 (Debian 12); contains QtWebEngineProcess, WebEngine resources,
      SQLite driver and OpenSSL TLS backend, no unresolved libraries. Fix on the way: `tools/appimage.sh` had no
      executable bit. Still open: Kevin starts it on the desktop (WebEngine sandbox inside the AppImage)
- [ ] Windows and macOS: not covered (other toolchains: MSVC/vcpkg, macOS bundle); separate decision

## M8: Ruumble moves into this repo (Kevin, 2026-11-04, agreed with Bert)

Goal: nothing depends on `Skrrytch/Ruumble` any more, neither the build nor the server image; that repo could be
deleted. Done on the local branch `merge-ruumble`, not pushed yet.

- [x] Desk moved into `desk/`, Ruumble merged at the repo root with its full history (`3cc31c2` = v0.32.1, 235
      commits, without Bert's tags so no old release runs again); LICENSE names Bert Speckels and Kevin Messerschmidt
- [x] Root `.gitattributes` keeps LF everywhere (`core.autocrlf=true` here checked Ruumble out with CRLF)
- [x] Desk panel is a normal commit in `web/`, `patches/ruumble` removed
- [x] Desk build uses the repo root as Ruumble; Mumble from `../../mumble` or, if missing, GitHub (`pins.env`
      `MUMBLE_URL`), also in the packaging container; version follows `bridge/package.json`, `packaging/VERSION` removed
- [x] References to `Skrrytch/Ruumble` replaced: image `ghcr.io/kevinwebman/ruumble`, demo
      `kevinwebman.github.io/Ruumble-Desk`, compose files, docs, `tools/release.sh`, release check; history (old
      changelog links, ADR-0009, charter) left as it was, ADR-0008 got a "current state" line
- [x] `release.yml`: job `appimage` (desk/package.sh appimage), attached as `Ruumble_Desk-<version>-x86_64.AppImage`
- [x] Checks 2026-11-04: root `pnpm lint && pnpm test && pnpm build` green (409 tests), `docker build -f
      deploy/Dockerfile .` ok, `./build.sh` ok (core 0.5.0 from the repo root), headless run connects, pairs and
      shows the Desk panel button, `MUMBLE_REPO=/nonexistent desk/package.sh appimage` ok (Mumble cloned from
      GitHub as in CI, 5.5 min). Not yet run on GitHub Actions: a manual dry run of `release.yml` would check that
- [ ] Kevin: push `merge-ruumble` to `main`; on GitHub enable Pages (source: GitHub Actions), keep issues on
      (release watch opens them), after the first release make the GHCR package `ruumble` public
- [ ] First release from this repo (`tools/release.sh 0.33.0`, tag `v0.33.0`): image, plugin, AppImage

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
- [ ] Remove the old `ruumble-mumble-fork` (Kevin decides)
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
  `schema/protocol.schema.json` is out of date on `main` (Node 24 here; maybe Node 22 differs).
- Roughly 50 changed lines in Mumble files (all under `#ifdef USE_RUUMBLE`) plus the CMake block;
  `patches/mumble` holds the touch points.
- After every `web` build the Ruumble service must be restarted (`test/local-stack.sh up`): it only serves the
  asset files that existed when it started, new hashed files get 404 and the page stays empty.
- Tray icon, global shortcuts and Mumble's own dialogs (settings, connect, certificate) stay Mumble's.
