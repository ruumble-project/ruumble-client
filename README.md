# Ruumble Client

A Mumble client with Ruumble built in: one window, the Ruumble building as the whole UI, Mumble's audio
underneath (local pilot of ADR-0024, draft in `docs/ADR-0024-draft.md`).

Ruumble itself (service, web UI, plugin) is [ruumble-server](https://github.com/ruumble-project/ruumble-server), pinned
here as the git submodule `server/` (`git clone --recurse-submodules`, or `git submodule update --init`). Mumble is
**used, never changed**: `tools/prepare.sh` clones it into `work/` (git-ignored), checks out the tag from `pins.env`
and applies `patches/mumble`. It is cloned from GitHub; `MUMBLE_REPO=<path>` uses a local checkout instead.
New Mumble releases are reported as an issue (`.github/workflows/mumble-release-watch.yml`). Take one over with
`tools/update-mumble.sh [tag]` (pin, 3-way merge of the patch, build, export; exit 3 = conflicts,
4 = build failed, fix in `work/mumble` then `tools/build.sh && tools/export-patches.sh`), or simply say "Update alles"
in Claude Code: the skill `.claude/skills/update-all` runs it, fixes conflicts and runs every check.

## For team members

> [!IMPORTANT]
> **[Open the team Kanban board](https://github.com/orgs/ruumble-project/projects/1)**: tickets of ruumble-client and ruumble-server, who works on what and the status of each.
> You need access to the Ruumble organization; if the page shows a 404, ask a maintainer to add you to the board.

```
./
  VERSION                  Ruumble Client's version
  pins.env                 Mumble tag that is built
  server/                  ruumble-server (submodule): plugin core, icon, service for the local stack
  client/                  Ruumble Client sources (linked into work/mumble/src/mumble/ruumble)
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
test/local-stack.sh up     # local Mumble server (Docker) + Ruumble service from server/
tools/run.sh               # Ruumble Client; the City Map opens: note 127.0.0.1:64738 as an address and enter
```

The **City Map** (`client/citymap/`, `client/CityMap`) takes the place of Mumble's connection dialog: addresses with
connection and visitors, and "On your next visit" (no building, the last visited one, or the headquarters, entered
automatically at start). Mumble's own server list stays in the Mumble menu (F10).

Build dependencies (Ubuntu/Mint, once): see `CLAUDE.md`.

## Program and AppImage

```sh
./build.sh                 # everything above in one go, the program in dist/ruumble-client
./build.sh --install       # plus start menu entry "Ruumble Client" and the command ruumble-client (~/.local/bin)
./build.sh --uninstall     # removes entry and command again
./build.sh --appimage      # plus dist/Ruumble_Client-x86_64.AppImage: one file with Qt and Qt WebEngine inside
```

`dist/ruumble-client` uses the Qt libraries of this system and needs the build dependencies installed.

Settings: Ruumble Client keeps its own (`~/.config/Ruumble/Ruumble Client/`), apart from the official Mumble. On its first
start it takes over the official client's audio settings and shortcuts once (marker file `mumble-settings-imported`;
delete it to import again) and its certificate (Mumble's automatic certificate backup).

## Packages for other machines (only Docker needed)

```sh
./package.sh appimage                  # dist/Ruumble_Client-x86_64.AppImage, one file with Qt and Qt WebEngine inside
./package.sh package --distro debian12     # .deb for Debian 12      sudo apt install ./dist/ruumble-client_*+debian12_amd64.deb
./package.sh package --distro ubuntu2404   # .deb for Ubuntu 24.04 and Mint 22
./package.sh package --distro fedora       # .rpm for the current Fedora   sudo dnf install ./dist/ruumble-client-*.rpm
./package.sh package --distro arch         # Arch Linux package    sudo pacman -U dist/ruumble-client-*.pkg.tar.zst
./package.sh all --distro debian12         # the package for that distro plus the AppImage
./package.sh every                         # both .deb, the .rpm, the Arch package and the AppImage
```

Everything builds in a container (`packaging/Dockerfile` with Debian or Ubuntu as base, `packaging/Dockerfile.fedora`, `packaging/Dockerfile.arch`), so the host needs nothing but
Docker; the repo and a local Mumble checkout are mounted read only, only `dist/` is written. The AppImage is always built on Debian 12
(glibc 2.36): it runs on Debian 12+, Ubuntu 22.10+, Mint 22+, current Fedora and Arch and similar. Native packages
are built against that distro's own Qt; their dependencies come from the linked libraries (`dpkg-shlibdeps`,
rpmbuild, `pacman -Qo`). Fedora's OpenSSL 4 needs an upstream Mumble fix that is not in a Mumble release yet: it is
kept in `patches/upstream` and applied before our touch points (`tools/prepare.sh`). They replace an installed `ruumble-desk`. The version is in `VERSION`.

Releases (details and when to release: [docs/releasing.md](docs/releasing.md)): `tools/release.sh <version>` bumps
`VERSION` and `CHANGELOG.md`; pushing the tag `v<version>` builds the
AppImage, both .deb, the .rpm and the Arch package in parallel in GitHub Actions (`.github/workflows/release.yml`) and attaches
them to this repo's GitHub release.

## NixOS and Nix

The flake in this repo builds Ruumble Client from source (Mumble at the tag in `pins.env` with our patches, Qt 6 with
WebEngine from nixpkgs), `x86_64-linux` and `aarch64-linux`:

```sh
nix run github:ruumble-project/ruumble-client             # try it
nix profile install github:ruumble-project/ruumble-client # install it for your user
```

In a NixOS configuration (flake):

```nix
{
  inputs.ruumble-client.url = "github:ruumble-project/ruumble-client";
  outputs = { nixpkgs, ruumble-client, ... }: {
    nixosConfigurations.<host> = nixpkgs.lib.nixosSystem {
      modules = [
        ruumble-client.nixosModules.default
        { programs.ruumble-client.enable = true; }
      ];
    };
  };
}
```

Or `nixpkgs.overlays = [ ruumble-client.overlays.default ];` and then `pkgs.ruumble-client` wherever you list packages.
The first build compiles Mumble (about 10 to 20 minutes); Qt comes from the NixOS binary cache. A release tag pins a
version: `github:ruumble-project/ruumble-client/v0.35.0`.

Without the flake, the AppImage works as well: `programs.appimage = { enable = true; binfmt = true; };` in the
configuration, then start the downloaded file (or `appimage-run Ruumble_Client-<version>-x86_64.AppImage`).

The build is `nix/package.nix`; `nix/mumble-src.json` pins the Mumble and IXWebSocket sources with their hashes.
`tools/update-mumble.sh` moves the Mumble tag there as well; the new hash is printed by the CI job "Nix"
(`.github/workflows/nix.yml`) or a local `nix build`.

## Windows

Not supported: the team dropped Windows after 0.35.1 (the last release with a Windows zip). The build that made it
is in the git history (`.github/workflows/windows.yml`, `packaging/windows/`, removed after `v0.35.1`).

`tools/appimage.sh` (also `./build.sh --appimage`, without container) downloads `linuxdeploy` and its Qt plugin
(pinned versions) into `work/appimage/tools` on first use.

## Change something

- Client code: edit `client/` (also reachable as `work/mumble/src/mumble/ruumble`), `tools/build.sh`.
- Ruumble web UI (Desk panel: `web/src/lib/desk/`, `web/src/lib/ui/desk/`), plugin core: edit in ruumble-server,
  then move the submodule here (`git -C server pull` or check out a tag, then commit `server`).
- Mumble touch points: edit in `work/mumble`, then `tools/export-patches.sh` writes them back to `patches/mumble`.
- After a web change: `test/local-stack.sh up` again (the service only serves the files it saw at start).
- Other Mumble tag: change `pins.env`, remove `work/mumble`, `tools/prepare.sh`; fix the patch in the work tree if
  it no longer applies and export it again.
