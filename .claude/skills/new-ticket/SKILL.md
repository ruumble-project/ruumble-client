---
name: new-ticket
description: Writes a well-formed GitHub issue for ruumble-server or ruumble-client from a description, a screenshot, an error message or a finding in the code - picks the type (feature, bug, task) and the repo, adds acceptance criteria and labels, checks for duplicates, shows the draft and files it only after confirmation. Use when someone says "/new-ticket ...", "leg ein Ticket an", "mach daraus ein Issue", "create an issue for ...".
---

# Write a ticket

Goal: a ticket another person (or Claude with `/ticket`) can work on without asking back. Rules in `docs/workflow.md` (and ruumble-server's, which it refers to).

## 1. Understand

- What is wanted or broken, and why. For a bug: what happened, what was expected, steps, versions (Ruumble, plugin,
  Mumble server and client, Ruumble Client, OS/browser). Look into the code to make it concrete: where it happens,
  which file, which ADR applies.
- Pick the repo: browser UI, service, plugin, deployment → `ruumble-project/ruumble-server`; the desktop program,
  its packages, Mumble updates → `ruumble-project/ruumble-client`. **One ticket per task**, even when it touches
  several parts (web, service, plugin, desk, client): never split it into one ticket per repo. File it in the repo
  where most of the work is (a feature that starts in the UI: ruumble-server), name the other parts in the body and
  give it the labels of every part.

## 2. Duplicates

```sh
gh issue list --repo ruumble-project/<repo> --state all --search "<keywords>"
```

A matching open ticket: suggest a comment there instead.

## 3. Draft

Follow the template of the type (`.github/ISSUE_TEMPLATE/`), in English:

- **Title**: short, what it is about ("Board export misses attachments").
- **Body**: what and why; for bugs steps and versions; **acceptance criteria** as a checklist, each one checkable;
  notes with file paths and related ADRs or tickets.
- **Labels**: `bug` / `enhancement` / `task` and the parts (`web`, `service`, `plugin`, `ops`, `client`);
  `needs-decision` when the approach is open; `good first issue` when small and clear.

Show the draft and ask whether to file it. Nothing is created without a yes.

## 4. File

```sh
gh issue create --repo ruumble-project/<repo> --title "<title>" --body-file <file> --label <a>,<b>
```

Labels that do not exist yet: leave them out and say so.

Then put it on the team Kanban board (project 1 of the organization; tickets are not added automatically), and check
that it is there:

```sh
gh project item-add 1 --owner ruumble-project --url <issue url>
gh project item-list 1 --owner ruumble-project --limit 200 --format json -q '.items[] | select(.content.number==<n>) | .status'
```

It starts in `Todo`. Report the link.
