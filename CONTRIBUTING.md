# Contributing to Ruumble Client

Thanks for your interest! Bug reports, ideas and pull requests are welcome.

- **Bugs and ideas** about the desktop program: open an [issue](https://github.com/ruumble-project/ruumble-client/issues/new/choose).
  About the building in the browser, the service or the plugin: [ruumble-server](https://github.com/ruumble-project/ruumble-server/issues/new/choose).
- **Security problems:** privately, through [ruumble-server's security advisories](https://github.com/ruumble-project/ruumble-server/security/advisories/new).

## How we work

Tickets, one branch per ticket and pull requests: [docs/workflow.md](docs/workflow.md). Outside contributors follow
the same flow from a fork; for anything bigger than a small fix, open an issue first so we can agree on the approach.

## Conventions

- **English everywhere**: code, comments, commit messages, docs.
- **Mumble stays unchanged** except for the touch points in `patches/mumble` (all under `#ifdef USE_RUUMBLE`).
- **Ruumble itself** (`server/`) is changed in ruumble-server, never here.
- More background: [CLAUDE.md](CLAUDE.md) (build, code rules, gotchas) and [README.md](README.md) (layout, tools).

By contributing you agree that your contribution is licensed under the [BSD-3-Clause license](LICENSE).
