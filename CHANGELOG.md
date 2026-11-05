# Changelog

All notable changes to Ruumble Client (formerly Ruumble Desk), the Mumble client with Ruumble built in. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), versions follow [Semantic Versioning](https://semver.org/).
Up to 0.33.1 Ruumble Desk was released together with Ruumble; those changes are in the changelog of
[ruumble-server](https://github.com/ruumble-project/ruumble-server/blob/main/CHANGELOG.md).

## [Unreleased]

## [0.36.0] - 2026-11-05

### Added

- Ruumble City Map: Mumble's connection dialog in the building look, in German and English. An address list with building name, address, connection (signal bars, milliseconds as tooltip) and visitors; note, edit and remove addresses (the same list as Mumble's favourites); "On your next visit": enter no building, the last visited one or the headquarters automatically at start. Entering automatically gives up after 15 s and shows the reason on the City Map instead of a Mumble dialog; Mumble's own server list stays in the Mumble menu (F10)

### Changed

- Built with Ruumble 0.35.0 (submodule `server/`, plugin core 0.5.0 unchanged)

### Fixed

- Entering a building whose address has an SRV record (`_mumble._tcp`) no longer waits 10 s when one of its servers is down: the servers are tried by their SRV priority, and an address that cannot be reached from here (e.g. IPv6 without an IPv6 route) is skipped right away instead of ending the attempt. A Mumble fix kept in `patches/upstream` until Mumble has it

### Removed

- Windows: the team dropped Windows support, and the Windows build is gone from the release workflow; no release ever had a Windows zip (0.35.0's Windows build failed, 0.35.1 was never published). Ruumble Client is for Linux (packages, AppImage, NixOS)

## [0.35.0] - 2026-11-05

### Added

- Fedora: an `.rpm` for the current Fedora in every release (built with Mumble's upstream fix for OpenSSL 4, kept in `patches/upstream` until a Mumble release has it)
- Windows: the release workflow builds Ruumble Client for Windows 10/11 x64 as a zip (`ruumble-client.exe` with Qt WebEngine, not signed yet); a release whose Windows build fails comes without it and says so
- NixOS: a Nix flake, `nix run github:ruumble-project/ruumble-client`, with a NixOS module (`programs.ruumble-client.enable`) and an overlay; built from source against Qt 6 from nixpkgs

## [0.34.0] - 2026-11-05

### Added

- Native packages in every release next to the AppImage: `.deb` for Debian 12 and for Ubuntu 24.04 / Linux Mint 22, and an Arch Linux package; all three replace an installed `ruumble-desk`

### Changed

- Built with Ruumble 0.34.0 (submodule `server/`, plugin core 0.5.0)
- Ruumble Desk has its own repository, `ruumble-project/ruumble-client`, its own version and its own releases; Ruumble (service, web UI, plugin core) is pinned as the git submodule `server/` (ADR-0025 in ruumble-server)
- Ruumble Desk is now called Ruumble Client: window title, start menu entry, command `ruumble-client`, AppImage `Ruumble_Client-<version>-x86_64.AppImage`, Debian package `ruumble-client` (replaces `ruumble-desk`). On its first start it takes over the settings and data of Ruumble Desk once (certificate, servers, pairing, the view's login), file by file without overwriting; marker `ruumble-desk-taken-over`
