---
name: ticket
description: Works on a GitHub issue of ruumble-client from start to a pushed commit on main - reads the ticket with its comments and acceptance criteria, checks it against the code, asks back when something is unclear, implements, builds and runs the client, updates changelog and docs, commits and pushes to main with "(fixes #<issue>)", or opens a pull request when asked. Use when someone says "/ticket 42", "arbeite Ticket 42 ab", "work on issue 42", "mach #42" or names an issue URL of ruumble-client.
---

# Work on a ticket

The team flow is `docs/workflow.md` (which refers to ruumble-server's); this skill follows it. Ask only when a decision
is really the person's (see "Stop and ask"); otherwise work through to the pushed commit.

## 1. Read the ticket

```sh
gh issue view <n> --comments
```

- If `gh` is missing or not logged in: say so (`sudo apt install gh`, `gh auth login`) and stop.
- The ticket is closed, labelled `needs-decision` or `blocked`, or assigned to someone else: say so and stop.
- Note the acceptance criteria. If a feature or task has none, write a proposal and ask before implementing.
- Linked tickets in ruumble-server: read them. If the change needs the web UI part or the plugin core, that work
  belongs in ruumble-server: say so, and work here only on the client side.

## 2. Check it against the code

- `CLAUDE.md` (scope, code rules) and `README.md` (layout, tools). Check the ticket's assumptions against the code;
  ask if they do not hold.
- Mumble touch point needed? Keep it minimal, under `#ifdef USE_RUUMBLE`, everything else in `client/`.
- Give a short plan (files, how to check it, open points) before writing code when it is more than a few lines.

## 3. Start on main

```sh
git switch main && git pull --ff-only && git submodule update --init
gh issue edit <n> --add-assignee @me
```

The team works on `main` directly. Only when the person wants a pull request (or several people work on the
ticket at once): `git switch -c <n>-<short-slug>`.

The working tree must be clean first; if not, show it and ask.

## 4. Implement

- Client code in `client/` (also reachable as `work/mumble/src/mumble/ruumble`). Mumble changes in `work/mumble`, then
  `tools/export-patches.sh`. Never edit `server/` or the patch file by hand.
- The `desk` contract (`client/DeskChannel.h`) only grows; older services must keep working.
- `CHANGELOG.md`: a line under `## [Unreleased]` for anything users notice. Docs (`README.md`, `CLAUDE.md`,
  `docs/touch-points.md`) when behaviour, tools or touch points change. No em or en dashes.

## 5. Checks

```sh
./build.sh                                   # must build
test/local-stack.sh up && test/run-desk.sh Anna fresh      # headless run, see test/README.md
node test/cdp.mjs eval "document.title"      # expect "... · Ruumble"; cdp.mjs shot <png> to look at it
kill $(cat test/.work/desk-Anna.pid); test/local-stack.sh down
./package.sh package --distro debian12       # packaging touched (also ubuntu2404, fedora, arch, appimage as fitting)
```

The Windows build cannot run locally: if `packaging/windows/` or Windows-specific code changed, push the branch under a
name starting with `windows` (or start `windows.yml` by hand) and check the result. Check every acceptance criterion.

## 6. Commit and push

- Commits: English, one line, ending with `(#<n>)`; the last one, which completes the ticket, with `(fixes #<n>)`.
- Personal settings that forbid pushing win: then stop here and name the commands below.

```sh
git push                                   # on main: done, CI runs on main
```

Only when a pull request was asked for: `git push -u origin HEAD` on the branch, then
`gh pr create --title "<what it does>" --body-file <file>` (no number in the title, GitHub adds the PR number). The body follows `.github/pull_request_template.md`: `Closes #<n>`, what and why, how it was tested, the checks ticked.

## 7. Report

Short, in the person's language: the commit (or PR link), what was done per acceptance criterion, the checks, what only a person
can check (start it on the desktop, speak, quit through the menu), follow-ups in ruumble-server.

## Stop and ask

- The ticket is unclear, has no acceptance criteria, or the code contradicts it.
- A Mumble touch point would change what the user sees of Mumble or how it behaves.
- The `desk` contract would change incompatibly, or the change needs ruumble-server first.
- A new build dependency or a change to the release workflow would be needed.
