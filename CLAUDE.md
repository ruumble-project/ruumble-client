# CLAUDE.md: ruumble-client

Ruumble Client is a Mumble client with Ruumble built in (pilot of ADR-0024, draft in `docs/ADR-0024-draft.md`).
This repo is `ruumble-project/ruumble-client`. Ruumble itself (service, web UI, plugin) is `ruumble-project/ruumble-server`,
pinned here as the git submodule `server/` (ADR-0025 in `server/docs/decisions/`); its rules are in `server/CLAUDE.md`.
After cloning: `git submodule update --init`.
Read `README.md` for the layout and the tools, `ROADMAP.md` for the state.

## The one rule

**Mumble is never changed.** `tools/prepare.sh` clones it from GitHub at the tag in `pins.env` into `work/mumble`
and applies `patches/mumble`; all Mumble work happens in `work/mumble` and goes back with `tools/export-patches.sh`.
Ruumble is edited in its own repo, never in `server/`; this repo only moves the submodule to a newer server commit.
New Mumble releases: "Update alles" runs the skill `.claude/skills/update-all` (`tools/update-mumble.sh`, fixes,
all checks).

Team workflow (`docs/workflow.md`, ADR-0026 in ruumble-server, section "Team and GitHub" below): tickets for the
overview, the team commits and pushes to `main` directly after the checks, pull requests only when someone wants a
second look or for outside contributors. Skills: `/ticket`, `/new-ticket`, `/update-all`. No em or en dashes anywhere;
code, comments and docs in English.

Progress lives in `ROADMAP.md`: tick off every finished item right away (`[x]`, `[~]` in progress) and add notes,
so Kevin or the next session can continue at any point.

## Scope

Works well **on this machine**: phases 0 and 1 of the ADR plus "Ruumble UI as host" (Kevin's choice):

- Ruumble's plugin core (`server/plugin/src/{core,config,discovery,messages,net}.cpp`, unchanged) is compiled
  into the client; `client/QtMumbleApi` implements its `MumbleApi` on the client's own classes.
- `client/RuumbleView`: the Ruumble web UI from the service is the whole window (QWebEngineView, persistent profile,
  pairing link opens in the view).
- `client/RuumbleIntegration`: hides Mumble's menu bar, toolbar and docks; Mumble's menus stay reachable as a native
  popup (F10, panel, user menu), the user list and Mumble's log and chat as optional docks (log and chat shown by
  themselves when the page has no Desk panel, i.e. an older service), settings with Ctrl+,. Right click in the building
  opens Mumble's own channel or user menu, people are dragged onto rooms (`showChannelMenu`, `showUserMenu`,
  `moveUser`; web side `web/src/lib/desk/admin.ts` in ruumble-server). Destructive actions only via the native menu.
- `client/DeskChannel`: object `desk` over QWebChannel (contract in `DeskChannel.h`), used by the web part in
  `web/src/lib/desk/` and `web/src/lib/ui/desk/DeskPanel.svelte` in ruumble-server. Keep both sides in sync, and keep
  the contract compatible: released clients meet newer services and the other way round.

Working assumptions from the ADR (the owner said "just do it"; the ADR stays a draft): own settings dir and name
"Ruumble Client", UI loaded from the service URL, Mumble 1.6 only. Not in scope: phase 2, screen sharing (needs Qt
newer than 6.4), Windows. The AppImage is built by `.github/workflows/release.yml` (version in `VERSION`, `tools/release.sh`).

## Code rules

- Mumble touch points stay minimal and under `#ifdef USE_RUUMBLE` (`patches/mumble`); everything else in `client/`.
- The core calls `MumbleApi` from its worker thread: every call runs on the main thread with an 800 ms timeout
  (`QtMumbleApi::onMain`), so shutdown cannot hang.
- The `desk` object only answers the service's own page (`RuumbleView::showsService`); log texts go to the page as
  plain text, never Mumble's HTML.

## Build dependencies (Mint 22.3 / Ubuntu 24.04, installed once)

```sh
sudo apt install build-essential cmake ninja-build pkgconf qt6-base-dev qt6-tools-dev \
  qt6-tools-dev-tools qt6-svg-dev qt6-l10n-tools qt6-webengine-dev qt6-webengine-dev-tools \
  qt6-webchannel-dev libgl-dev libboost-dev libssl-dev libprotobuf-dev protobuf-compiler \
  libprotoc-dev libcap-dev libxi-dev libasound2-dev libpulse-dev libogg-dev libsndfile1-dev \
  libopus-dev libpoco-dev nlohmann-json3-dev libcli11-dev libutfcpp-dev libspdlog-dev libspeexdsp-dev
```

## Gotchas

- After a web change: `test/local-stack.sh up` again; the service only serves the asset files it saw at start (new
  hashed files get 404, the page stays empty).
- Web checks happen in ruumble-server: `corepack pnpm lint && corepack pnpm test` there.
- Git: `core.autocrlf=true` on this machine; the root `.gitattributes` keeps every file LF.
- Mumble in Docker sees the host as the Docker gateway (172.x): the local service runs with `ADDRESS_CHECK=warn`.
- Headless tests (`test/run-desk.sh`) use an isolated HOME; Playwright cannot attach to QtWebEngine, `test/cdp.mjs`
  speaks raw CDP. Rooms are entered by double-click.

## Team and GitHub (read before any git or gh action)

Several people work on this repo, each with their own Claude Code; these rules hold for every instance, whatever the
personal settings say about style. The full flow is `docs/workflow.md`.

- **Start of a session**: `gh auth status` (the skills need the GitHub CLI; if it is not logged in, ask the person to
  run `gh auth login`), `git status`, the checked-out branch, and `git pull --ff-only` on `main`.
- **Tickets** keep the overview: work that takes more than a few minutes has a GitHub issue; offer `/new-ticket` when
  there is none. Skills: `/ticket <n>` works an issue to a commit on `main`, `/new-ticket` writes one.
- **The team works on `main` directly** (Kevin, Simon, Bert; GitHub roles admin, maintain, write): pull, commit, run
  the checks (`./build.sh` and a start of the client (plus a package build when packaging changed)), push. No pull request and no approval needed. Commits are one line in English, ending
  with `(#<issue>)`; the commit that completes a ticket ends with `(fixes #<issue>)`, so GitHub closes it. CI runs on
  every push to `main`; if it turns red, fixing it comes first.
- **Pull requests are optional** inside the team: open one when the person wants a second look (bigger changes,
  something risky), on a branch `<issue>-<slug>`; the author merges with *Squash and merge* once CI is green (the
  title, one line without a number, becomes the commit; GitHub adds the PR number). Several people working on the
  same ticket at once also use a branch.
- **What GitHub enforces** (ruleset on `main`): no force pushes, no deletion of `main`; everyone else (outside
  contributors without write access, working from a fork) needs a pull request with the required checks green
  (`Build (Debian 12 package)`; `windows.yml` and `nix.yml` also
  run when their parts change) and a team member to merge it. The team roles bypass this.
- **Claude** pushes to `main` when it works for a team member and the checks pass; it never force-pushes and never
  rewrites published history. Pushing counts as asked when the person asked for the change or used `/ticket`;
  personal settings that forbid pushing win (then name the commands instead). A rejected push means the account is not
  in the team: push a branch and open a pull request instead.
- **Labels**: `bug`, `enhancement`, `task` (kind), `client`, `web`, `service`, `plugin`, `ops` (part), `needs-decision`
  and `blocked` (do not start), `good first issue`. Ask before working on a ticket assigned to someone else.
- **Writing style** in everything committed: English, no em or en dashes (use a hyphen with spaces, a colon or a new
  sentence), commit messages one line without trailers.
