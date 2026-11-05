#!/usr/bin/env bash
# Builds the .rpm from work/build/mumble on Fedora; rpmbuild derives the library dependencies itself.
#   make-rpm.sh <distro-label> <out-dir>
set -eu
HERE="$(cd "$(dirname "$0")" && pwd)"
source "$HERE/../tools/common.sh"; source "$HERE/desktop-entry.sh"; source "$HERE/stage.sh"
LABEL=$1 OUT=$2
R="$W/rpm"
rm -rf "$R" && mkdir -p "$R"/{BUILD,RPMS,SOURCES,SPECS,SRPMS,stage}
stage_files "$R/stage"
cat > "$R/SPECS/ruumble-client.spec" <<SPEC
Name:           ruumble-client
Version:        $VERSION
Release:        1.$LABEL
Summary:        $DESCRIPTION_SHORT
License:        BSD-3-Clause AND LGPL-3.0-only
URL:            https://github.com/ruumble-project/ruumble-client
# loaded at run time, not visible as a library dependency
Requires:       qt6-qtbase-gui qt6-qtwebengine
Recommends:     pipewire-pulseaudio
Provides:       ruumble-desk = %{version}-%{release}
Obsoletes:      ruumble-desk < %{version}-%{release}

%description
$DESCRIPTION_LONG

%install
cp -a $R/stage/. %{buildroot}/

%files
/usr/bin/ruumble-client
/usr/share/applications/ruumble-client.desktop
/usr/share/icons/hicolor/scalable/apps/ruumble-client.svg
%license /usr/share/licenses/ruumble-client/NOTICE
SPEC
rpmbuild --define "_topdir $R" --define "debug_package %{nil}" --define "__strip /bin/true" -bb "$R/SPECS/ruumble-client.spec"
mkdir -p "$OUT"
cp "$R"/RPMS/*/ruumble-client-*.rpm "$OUT/"
echo "package: $(ls "$OUT"/ruumble-client-*"$LABEL"*.rpm)"
