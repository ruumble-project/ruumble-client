# Changelog

All notable changes to Ruumble Client (formerly Ruumble Desk), the Mumble client with Ruumble built in. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), versions follow [Semantic Versioning](https://semver.org/).
Up to 0.33.1 Ruumble Desk was released together with Ruumble; those changes are in the changelog of
[ruumble-server](https://github.com/ruumble-project/ruumble-server/blob/main/CHANGELOG.md).

## [Unreleased]

### Changed

- Built with Ruumble 0.34.0 (submodule `server/`, plugin core 0.5.0)
- Ruumble Desk has its own repository, `ruumble-project/ruumble-client`, its own version and its own releases; Ruumble (service, web UI, plugin core) is pinned as the git submodule `server/` (ADR-0025 in ruumble-server)
- Ruumble Desk is now called Ruumble Client: window title, start menu entry, command `ruumble-client`, AppImage `Ruumble_Client-<version>-x86_64.AppImage`, Debian package `ruumble-client` (replaces `ruumble-desk`). On its first start it takes over the settings and data of Ruumble Desk once (certificate, servers, pairing, the view's login), file by file without overwriting; marker `ruumble-desk-taken-over`
