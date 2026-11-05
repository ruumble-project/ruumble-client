#!/usr/bin/env bash
# Prepares a Ruumble Client release: bumps VERSION, dates the Unreleased section of CHANGELOG.md and commits "Release <version>".
#   tools/release.sh <version>      e.g. 0.34.0
# Then push, and push the tag v<version>: .github/workflows/release.yml builds the AppImage and creates the GitHub release.
set -euo pipefail
cd "$(dirname "$0")/.."
die() { echo "release: $*" >&2; exit 1; }

version="${1:-}"
[[ "$version" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || die "usage: tools/release.sh <major.minor.patch>"
[[ -z "$(git status --porcelain)" ]] || die "the working tree is not clean"
[[ "$(git branch --show-current)" == main ]] || die "not on main"
git rev-parse -q --verify "refs/tags/v$version" >/dev/null && die "tag v$version exists"

unreleased="$(awk '/^## \[Unreleased\]/{on=1; next} /^## \[/{on=0} on' CHANGELOG.md | grep -v '^[[:space:]]*$' || true)"
[[ -n "$unreleased" ]] || die "CHANGELOG.md: nothing under ## [Unreleased]"

echo "$version" > VERSION
today="$(date +%Y-%m-%d)"
awk -v v="$version" -v d="$today" '
  /^## \[Unreleased\]/ { print; print ""; print "## [" v "] - " d; next }
  { print }
' CHANGELOG.md > CHANGELOG.md.tmp && mv CHANGELOG.md.tmp CHANGELOG.md

git add VERSION CHANGELOG.md
git commit -q -m "Release $version"
echo "Committed \"Release $version\". Next:"
echo "  git push && git tag v$version && git push origin v$version"
