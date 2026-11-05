# Releasing a new version

How to publish a new Ruumble Client version on GitHub. The flow is the same as in ruumble-server
([its releasing.md](https://github.com/ruumble-project/ruumble-server/blob/main/docs/releasing.md)), smaller.

## In short

```sh
# on main, working tree clean, CHANGELOG.md has entries under ## [Unreleased]
tools/release.sh 0.36.0            # bumps VERSION, dates the changelog section, commits "Release 0.36.0"
git push origin main
git tag v0.36.0
git push origin v0.36.0            # starts .github/workflows/release.yml, which builds and publishes everything
```

The GitHub release is made only by the workflow; never create one by hand.

## When to release, and the server's version

- **A release only when something changed that users notice**: the client itself, its packages, a newer Mumble, or a
  newer `server/` submodule that brings a changed plugin core or icon. Changes to docs, tickets, CI, `CLAUDE.md` or the
  Claude skills get no release; they reach everyone through `main`.
- **Client and server have their own versions** (ADR-0025 in ruumble-server). The numbers do not have to match: never
  bump one only to match the other.
- **Compatibility does not come from the numbers**: a client works with older and newer services (the `desk` contract
  only grows). The changelog names the server version the client is built with ("Built with Ruumble 0.34.0"), which is
  the tag the `server/` submodule points to. To take over a server release: `git -C server fetch --tags && git -C
  server checkout v<version>`, commit `server` ("Ruumble <version> as server/"), add the changelog line.

Choosing the number ([Semantic Versioning](https://semver.org/)): patch for fixes, minor for new features or packages.

## What the workflow builds

Pushing the tag `v<version>` starts **Release** (`.github/workflows/release.yml`). It checks that the tag matches
`VERSION` and that `CHANGELOG.md` has the section, then builds in parallel:

| Asset | Built by |
|---|---|
| `Ruumble_Client-<version>-x86_64.AppImage` | `./package.sh appimage` (Debian 12 container) |
| `ruumble-client_<version>+debian12_amd64.deb`, `..._+ubuntu2404_amd64.deb` | `./package.sh package --distro debian12` / `ubuntu2404` |
| `ruumble-client-<version>-1.fc<NN>.x86_64.rpm` | `./package.sh package --distro fedora` |
| `ruumble-client-<version>-1-x86_64.pkg.tar.zst` | `./package.sh package --distro arch` |
| `SHA256SUMS` | the release job |

The release appears only when every build succeeded. There is no Windows build: the team dropped Windows after
0.35.1. NixOS needs no asset: the tag itself is
the pinned version (`nix run github:ruumble-project/ruumble-client/v<version>`); the notes name it.

Started by hand (Actions, *Release*, *Run workflow*) it is a dry run: everything is built and uploaded as a workflow
artifact, no release.

## Check the result

```sh
gh run list --workflow release.yml --limit 1
gh release view v<version>
```

Expected: the release "Ruumble Client <version>" with the assets above.

## When the workflow failed

- **A Linux build failed, no release was created**: fix it on `main`, then move the tag to the fixed commit:
  `git push --delete origin v<version> && git tag -f v<version> && git push origin v<version>`. Only while no release
  exists for that tag.
- **The release exists but something is wrong in it**: never move the tag; fix it on `main` and release the next patch
  version.
