# Ruumble Desk

A Mumble client with Ruumble built in: one window, the Ruumble building as the whole UI, Mumble's audio
underneath (local pilot of ADR-0024, draft in `docs/ADR-0024-draft.md`).

Ruumble itself (service, web UI, plugin) is the root of this repo; this folder `desk/` holds the client. Mumble is
**used, never changed**: `tools/prepare.sh` clones it into `work/` (git-ignored), checks out the tag from `pins.env`
and applies `patches/mumble`. It is cloned from GitHub; `MUMBLE_REPO=<path>` uses a local checkout instead.
New Mumble releases are reported as an issue (`.github/workflows/mumble-release-watch.yml`). Take one over with
`tools/update-mumble.sh [tag]` (interface files, pin, 3-way merge of the patch, build, export; exit 3 = conflicts,
4 = build failed, fix in `work/mumble` then `tools/build.sh && tools/export-patches.sh`), or simply say "Update alles"
in Claude Code: the skill `.claude/skills/update-all` runs it, fixes conflicts and runs every check.

```
desk/
  pins.env                 Mumble tag that is built
  client/                  Ruumble Desk sources (linked into work/mumble/src/mumble/ruumble)
  patches/mumble/          touch points in Mumble files (all under #ifdef USE_RUUMBLE) + CMake block,
                           listed in docs/touch-points.md
  tools/                   prepare, build, run, export-patches, appimage
  packaging/               container builds for the AppImage and the native packages
  test/                    local test stack and headless tests (see test/README.md)
  docs/                    ADR draft, touch points
  ROADMAP.md               progress, ticked off as it goes
```

## Use

```sh
tools/prepare.sh           # work/mumble: Mumble from GitHub at the pinned tag, plus the patch
tools/build.sh             # client into work/build (first time ~10 min)
test/local-stack.sh up     # local Mumble server (Docker) + Ruumble service from the repo root
tools/run.sh               # Ruumble Desk; the server manager opens, connect to 127.0.0.1:64738
```

Build dependencies (Ubuntu/Mint, once): see `CLAUDE.md`.

## Program and AppImage

```sh
./build.sh                 # everything above in one go, the program in dist/ruumble-desk
./build.sh --install       # plus start menu entry "Ruumble Desk" and the command ruumble-desk (~/.local/bin)
./build.sh --uninstall     # removes entry and command again
./build.sh --appimage      # plus dist/Ruumble_Desk-x86_64.AppImage: one file with Qt and Qt WebEngine inside
```

`dist/ruumble-desk` uses the Qt libraries of this system and needs the build dependencies installed.

Settings: Ruumble Desk keeps its own (`~/.config/Ruumble/Ruumble Desk/`), apart from the official Mumble. On its first
start it takes over the official client's audio settings and shortcuts once (marker file `mumble-settings-imported`;
delete it to import again) and its certificate (Mumble's automatic certificate backup).

## Packages for other machines (only Docker needed)

```sh
./package.sh appimage                  # dist/Ruumble_Desk-x86_64.AppImage, one file with Qt and Qt WebEngine inside
./package.sh package --distro debian12     # .deb for Debian 12      sudo apt install ./dist/ruumble-desk_*+debian12_amd64.deb
./package.sh package --distro ubuntu2404   # .deb for Ubuntu 24.04 and Mint 22
./package.sh all --distro debian12         # the .deb for that distro plus the AppImage
./package.sh every                         # both .deb plus the AppImage
```

Everything builds in a container (`packaging/Dockerfile`, Debian or Ubuntu as base), so the host needs nothing but
Docker; the repo and a local Mumble checkout are mounted read only, only `dist/` is written. The AppImage is always built on Debian 12
(glibc 2.36): it runs on Debian 12+, Ubuntu 22.10+, Mint 22+, current Fedora and Arch and similar, so only Debian
and Ubuntu get a native package. The .deb is built against that distro's own Qt; its dependencies come from
`dpkg-shlibdeps`. The version is Ruumble's (`bridge/package.json`).

Releases: pushing a tag `v<version>` builds the AppImage in GitHub Actions (`.github/workflows/release.yml` at the
repo root) and attaches it to the GitHub release next to the server image and the plugin.

`tools/appimage.sh` (also `./build.sh --appimage`, without container) downloads `linuxdeploy` and its Qt plugin
(pinned versions) into `work/appimage/tools` on first use.

## Change something

- Client code: edit `client/` (also reachable as `work/mumble/src/mumble/ruumble`), `tools/build.sh`.
- Ruumble web UI (Desk panel: `web/src/lib/desk/`, `web/src/lib/ui/desk/`): edit at the repo root as usual.
- Mumble touch points: edit in `work/mumble`, then `tools/export-patches.sh` writes them back to `patches/mumble`.
- After a web change: `test/local-stack.sh up` again (the service only serves the files it saw at start).
- Other Mumble tag: change `pins.env`, remove `work/mumble`, `tools/prepare.sh`; fix the patch in the work tree if
  it no longer applies and export it again.
