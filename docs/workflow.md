# Working together: tickets, branches, pull requests

The team flow is the same in both repositories and described once in ruumble-server:
[docs/workflow.md](https://github.com/ruumble-project/ruumble-server/blob/main/docs/workflow.md) (decision: ADR-0026
there). In short: ticket first, a branch `<issue>-<slug>` from `main`, one-line English commits ending with
`(#<issue>)`, a pull request with `Closes #<issue>`, green CI, one approval, squash merge.

What is different here:

## Where a ticket goes

Tickets about the desktop program, its packages (AppImage, .deb, .rpm, Arch, Windows), Mumble updates and the
Mumble patch belong here. The web UI shown inside the client (`web/src/lib/desk/`, `web/src/lib/ui/desk/`), the plugin
core and the service live in ruumble-server: a change there gets a ticket in that repo, linked from here.

A change to the `desk` QWebChannel object (`client/DeskChannel.h`) needs both sides: one ticket per repo, the server
side merged first and compatible with released clients, then the client.

## Checks before a pull request

| Changed | Run |
|---|---|
| `client/`, `patches/`, `tools/` | `./build.sh`, then start it: `tools/run.sh` (or headless: `test/local-stack.sh up` and `test/run-desk.sh Anna fresh`, see `test/README.md`) |
| Mumble patch (`patches/mumble`) | edit in `work/mumble`, then `tools/export-patches.sh`; never edit the patch file by hand |
| packaging (`packaging/`, `package.sh`) | `./package.sh package --distro <debian12, ubuntu2404, fedora or arch>` or `./package.sh appimage` |
| Windows build (`packaging/windows/`) | push the branch: `.github/workflows/windows.yml` runs on branches named `windows*`, or start it by hand under Actions |
| `server/` submodule | a pull request of its own: "Ruumble <version> as server/ (#<issue>)", after the server release |

CI builds the client and the Debian 12 package on every pull request (`.github/workflows/ci.yml`).

## Rules that only apply here

- **Mumble is never changed** outside `patches/mumble` (touch points under `#ifdef USE_RUUMBLE`, listed in
  `docs/touch-points.md`). Upstream Mumble fixes we need before a Mumble release go to `patches/upstream`.
- **Never edit `server/`** in this repo; change ruumble-server and move the submodule.
- **Compatibility**: a released client meets newer services, a newer client meets older services. The `desk` contract
  only grows.

## Working with Claude Code

`/ticket 42` and `/new-ticket <description>` work here as in ruumble-server (skills in `.claude/skills/`); `/update-all`
takes over a new Mumble release. They need the GitHub CLI (`gh auth login`).

## Setup for a new colleague

```sh
git clone --recurse-submodules git@github.com:ruumble-project/ruumble-client.git
cd ruumble-client
# build dependencies once: CLAUDE.md, "Build dependencies"
./build.sh
```
