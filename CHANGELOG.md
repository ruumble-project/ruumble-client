# Changelog

All notable changes to Ruumble Client (formerly Ruumble Desk), the Mumble client with Ruumble built in. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), versions follow [Semantic Versioning](https://semver.org/).
Up to 0.33.1 Ruumble Desk was released together with Ruumble; those changes are in the changelog of
[ruumble-server](https://github.com/ruumble-project/ruumble-server/blob/main/CHANGELOG.md).

## [Unreleased]

## [0.35.1] - 2026-11-05

### Added

- Windows: Ruumble Client for Windows 10/11 x64 is part of the release now, as `Ruumble_Client-<version>-windows-x64.zip` (unpack, start `ruumble-client.exe`; not signed yet, so SmartScreen asks once)

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
