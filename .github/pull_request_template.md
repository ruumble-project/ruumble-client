<!-- Title: one line in English, what the change does, ending with the ticket, e.g. "Settings open with Ctrl+, on Windows too (#42)". It becomes the squash commit on main. -->

Closes #

## What and why

<!-- What does this change and why; anything the reviewer should look at first. -->

## How it was tested

<!-- Built with ./build.sh and started? Headless run? Which distro or Windows? Screenshots for visible changes. -->

## Checks

- [ ] `./build.sh` builds and the client starts, connects and shows the building
- [ ] Mumble patch changed only through `work/mumble` and `tools/export-patches.sh`, touch points under `#ifdef USE_RUUMBLE`
- [ ] Package built (`./package.sh ...`), if packaging changed; Windows workflow green, if `packaging/windows/` changed
- [ ] `CHANGELOG.md` has a line under `## [Unreleased]` (if users notice it)
- [ ] Docs updated (`README.md`, `CLAUDE.md`, `docs/`)
- [ ] Linked ticket in ruumble-server, if the web UI part or the `desk` contract changed
