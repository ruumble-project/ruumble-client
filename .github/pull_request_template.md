<!-- Title: one line in English, what the change does, without a number, e.g. "Settings open with Ctrl+, from the panel too". It becomes the squash commit on main; GitHub adds the pull request number, "Closes #" below links the ticket. -->

Closes #

## What and why

<!-- What does this change and why; anything the reviewer should look at first. -->

## How it was tested

<!-- Built with ./build.sh and started? Headless run? Which distro? Screenshots for visible changes. -->

## Checks

- [ ] `./build.sh` builds and the client starts, connects and shows the building
- [ ] Mumble patch changed only through `work/mumble` and `tools/export-patches.sh`, touch points under `#ifdef USE_RUUMBLE`
- [ ] Package built (`./package.sh ...`), if packaging changed
- [ ] `CHANGELOG.md` has a line under `## [Unreleased]` (if users notice it)
- [ ] Docs updated (`README.md`, `CLAUDE.md`, `docs/`)
- [ ] Linked ticket in ruumble-server, if the web UI part or the `desk` contract changed
