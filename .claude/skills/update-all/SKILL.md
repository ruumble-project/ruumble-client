---
name: update-all
description: Brings Ruumble Client up to date in one go - takes over a new Mumble release (Mumble tag and patch, fixing conflicts and build errors), optionally moves the server submodule, then runs every check (Desk build, headless run, AppImage) and reports. Use when someone says "Update alles", "alles aktualisieren", "update everything", "Mumble aktualisieren", "neues Mumble übernehmen", or after an issue "Mumble ... available".
---

# Update everything

Goal: the person says one sentence, you do the rest and come back with a short report. Work through the steps in
order, fix what breaks on the way, and only stop to ask when a decision is really theirs (see "Stop and ask").

Ruumble's interface files, service, web UI and plugin live in ruumble-server (submodule `server/`), which has its
own `update-all` skill. Never edit `server/` here.

## 0. Starting point

- `git status --short` must be clean; if not, show what is there and ask whether to continue on top of it.
- Be on `main`; `git pull --ff-only` if a remote is set and the person usually pulls (ask once if unsure).
- `git submodule update --init`.
- Note the current state: `grep MUMBLE_REF pins.env`, `git -C server describe --tags --always`, latest Mumble
  release: `curl -fsSL https://api.github.com/repos/mumble-voip/mumble/releases/latest | grep tag_name`.

## 1. Mumble

If the latest release is newer than `MUMBLE_REF` (compare with `sort -V`; ignore pre-releases and older branches):

```sh
tools/update-mumble.sh <tag>      # ~10 to 20 min: clone, 3-way apply, build; run it in the background
```

- Exit 0: done, the patch was refreshed by `tools/export-patches.sh`.
- Exit 3 (patch conflicts): resolve the conflict markers in `work/mumble` (`git -C work/mumble status`) so every touch
  point does what it did before. Then `tools/build.sh`.
- Exit 4 (build failed, log `work/build.log`): fix the cause in `work/mumble` (touch points) or in `client/` (follow
  renamed or changed Mumble APIs). Repeat `tools/build.sh` until it passes.
- After 3 or 4: `tools/export-patches.sh`.
- Rules (from `CLAUDE.md`): Mumble touch points stay minimal and under `#ifdef USE_RUUMBLE`; never change Mumble's
  behaviour beyond that; `work/` is scratch, only `patches/mumble` and `client/` are kept.
- If `MumblePlugin.h` changed between the tags, check `client/QtMumbleApi.*`.
- Add a line under `## [Unreleased]` in `CHANGELOG.md` ("Built on Mumble <tag>") and tick a line in `ROADMAP.md`.

If Mumble is already current, say so and go on with the checks (they still prove the tree is healthy).

## 2. Server submodule

If ruumble-server has a newer release tag than `server/` (`git -C server fetch --tags`), say so and ask whether to move
it (`git -C server checkout <tag>`, then commit `server`). Moving it changes the plugin core built into the Desk.

## 3. Checks (all must pass)

```sh
./build.sh                                    # Desk into dist/ (skip if step 1 just built it)
```

Headless run of the Desk against the local stack (see `test/README.md`):

```sh
test/local-stack.sh up
test/run-desk.sh Anna fresh
# wait until curl -s http://127.0.0.1:64080/api/version shows a plugin version, then:
node test/cdp.mjs eval "document.title"   # expect "... · Ruumble"
node test/cdp.mjs shot <scratch>/desk.png  # look at it: building, top bar, Desk panel button
kill $(cat test/.work/desk-Anna.pid); test/local-stack.sh down
```

AppImage (only Docker needed, ~6 min, background): `./package.sh appimage`, then check
`dist/Ruumble_Client-x86_64.AppImage` exists.

A check that fails is fixed, not skipped. If a fix would change behaviour (not just follow Mumble), stop and ask.

## 4. Report

Short, in the person's language:
- Mumble: old tag, new tag (or "already current"), whether the patch needed work and what was changed.
- Server submodule: where it stands, whether it was moved.
- Each check with its result.
- What only a person can do: start the Desk once on the desktop (`tools/run.sh`, speak, quit through the menu).
- A one-line commit message. Do not commit or push unless the person asks; then on a branch `mumble-<tag>` with a pull request (`docs/workflow.md`), pushed only when
  told to.

## Stop and ask

- The new Mumble release is a new major or minor line (e.g. 1.6 to 1.7): confirm before moving the pin.
- A touch point cannot be kept without changing what the user sees or how Mumble behaves.
- Something outside this procedure would have to change (Ruumble features, CI, release process).
