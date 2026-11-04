# Ruumble Desk

A Mumble client with Ruumble built in: one window, the Ruumble building as the whole UI, Mumble's audio
underneath (local pilot of ADR-0024, draft in `docs/ADR-0024-draft.md`).

This repo only holds our own work. Mumble and Ruumble are **used, never changed**: `tools/prepare.sh` clones
both from the checkouts next to this repo into `work/` (git-ignored), checks out the pinned refs from `pins.env`
and applies the patches.

```
ruumble-desk/
  pins.env                 Mumble tag and Ruumble commit that are built
  client/                  Ruumble Desk sources (linked into work/mumble/src/mumble/ruumble)
  patches/mumble/          touch points in Mumble files (all under #ifdef USE_RUUMBLE) + CMake block
  patches/ruumble/         Desk panel in the Ruumble web UI (only active inside Ruumble Desk)
  tools/                   prepare, build, run, export-patches
  test/                    local test stack and headless tests (see test/README.md)
  docs/                    ADR draft
  ROADMAP.md               progress, ticked off as it goes
```

## Use

```sh
tools/prepare.sh           # work/mumble and work/Ruumble from ../mumble and ../Ruumble (read only)
tools/build.sh             # client into work/build (first time ~10 min)
test/local-stack.sh up     # local Mumble server (Docker) + Ruumble service from work/Ruumble
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

## Packages for other machines (only Docker needed)

```sh
./package.sh appimage                  # dist/Ruumble_Desk-x86_64.AppImage, one file with Qt and Qt WebEngine inside
./package.sh package --distro debian12     # .deb for Debian 12      sudo apt install ./dist/ruumble-desk_*+debian12_amd64.deb
./package.sh package --distro ubuntu2404   # .deb for Ubuntu 24.04 and Mint 22
./package.sh package --distro fedora       # .rpm for the current Fedora   sudo dnf install ./dist/ruumble-desk-*.rpm
./package.sh package --distro arch         # Arch Linux package      sudo pacman -U dist/ruumble-desk-*.pkg.tar.zst
./package.sh all --distro arch             # the package for that distro plus the AppImage
./package.sh every                         # all four packages plus the AppImage
```

Everything builds in a container (`packaging/Dockerfile*`, one per distro family), so the host needs nothing but
Docker; Mumble and Ruumble are mounted read only, only `dist/` is written. The AppImage is always built on Debian 12
(glibc 2.36): it runs on Debian 12+, Ubuntu 22.10+, Mint 22+, current Fedora and Arch and similar. Native packages
are built per distro against that distro's own Qt; their dependencies are derived from the linked libraries
(`dpkg-shlibdeps`, rpmbuild's automatic requires, `pacman -Qo`). The version is in `packaging/VERSION`.

`tools/appimage.sh` (also `./build.sh --appimage`, without container) downloads `linuxdeploy` and its Qt plugin
(pinned versions) into `work/appimage/tools` on first use.

## Change something

- Client code: edit `client/` (also reachable as `work/mumble/src/mumble/ruumble`), `tools/build.sh`.
- Mumble touch points or the Ruumble web UI: edit in `work/mumble` or `work/Ruumble`, then
  `tools/export-patches.sh` writes them back to `patches/`. Commit only this repo.
- After a web change: `test/local-stack.sh up` again (the service only serves the files it saw at start).
- Other Mumble tag or Ruumble commit: change `pins.env`, remove the work tree, `tools/prepare.sh`; fix patches
  that no longer apply in the work tree and export them again.
