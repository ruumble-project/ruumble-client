# Ruumble Client for Nix: Mumble at the tag in nix/mumble-src.json (the same as pins.env) with patches/upstream and
# patches/mumble, the client sources from client/ and the Ruumble core from the server/ submodule, built against Qt 6
# with WebEngine from nixpkgs. Everything is fetched with a hash, so the build needs no network (IXWebSocket, which the
# patch fetches with FetchContent, is handed in as a source directory).
# After a Mumble update: tools/update-mumble.sh moves pins.env; nix/mumble-src.json needs the same rev and its new hash
# (`nix build` prints the right one when it does not match), CI checks that both agree.
{
  lib,
  stdenv,
  fetchFromGitHub,
  cmake,
  ninja,
  pkg-config,
  python3,
  qt6,
  boost,
  poco,
  protobuf,
  openssl,
  libsndfile,
  libopus,
  libogg,
  libvorbis,
  flac,
  rnnoise,
  alsa-lib,
  libpulseaudio,
  pipewire,
  libcap,
  xorg,
}:

let
  root = ../.;
  pin = lib.importJSON ./mumble-src.json;
  mumbleSrc = fetchFromGitHub {
    owner = "mumble-voip";
    repo = "mumble";
    rev = pin.rev;
    hash = pin.hash;
    fetchSubmodules = true;
  };
  ixwebsocket = fetchFromGitHub {
    owner = "machinezone";
    repo = "IXWebSocket";
    rev = pin.ixwebsocket.rev;
    hash = pin.ixwebsocket.hash;
  };
  upstreamPatches = map (f: root + "/patches/upstream/${f}") (
    builtins.filter (f: lib.hasSuffix ".patch" f) (builtins.attrNames (builtins.readDir (root + "/patches/upstream")))
  );
  buildNumber = lib.last (lib.splitString "." pin.rev);
in
stdenv.mkDerivation {
  pname = "ruumble-client";
  version = lib.fileContents (root + "/VERSION");

  src = mumbleSrc;

  # upstream backports first, then our touch points (as tools/prepare.sh does)
  patches = upstreamPatches ++ [ (root + "/patches/mumble/0001-ruumble-desk-touch-points.patch") ];

  postPatch = ''
    cp -r ${root + "/client"} src/mumble/ruumble
    chmod -R u+w src/mumble/ruumble
  '';

  nativeBuildInputs = [
    cmake
    ninja
    pkg-config
    python3
    protobuf
    qt6.qttools
    qt6.wrapQtAppsHook
  ];

  buildInputs = [
    qt6.qtbase
    qt6.qtsvg
    qt6.qtwebengine
    qt6.qtwebchannel
    boost
    poco
    protobuf
    openssl
    libsndfile
    libopus
    libogg
    libvorbis
    flac
    rnnoise
    alsa-lib
    libpulseaudio
    pipewire
    libcap
    xorg.libX11
    xorg.libXi
    xorg.libXext
  ];

  # the same options as tools/build.sh, plus what the sandbox needs
  cmakeFlags = [
    "-Dserver=OFF"
    "-Dtests=OFF"
    "-Doverlay-xcompile=OFF"
    "-Dzeroconf=OFF"
    "-Dspeechd=OFF"
    "-Djackaudio=OFF"
    "-Dportaudio=OFF"
    "-Dupdate=OFF"
    "-Dtranslations=OFF"
    "-Dwarnings-as-errors=OFF"
    "-Duse-timestamps=OFF"
    # the bundled RNNoise downloads its model while configuring: take the one from nixpkgs
    "-Dbundled-rnnoise=OFF"
    "-DBUILD_NUMBER=${buildNumber}"
    "-DRUUMBLE_SOURCE_DIR=${root + "/server"}"
    "-DFETCHCONTENT_FULLY_DISCONNECTED=ON"
    "-DFETCHCONTENT_SOURCE_DIR_IXWEBSOCKET=${ixwebsocket}"
  ];

  installPhase = ''
    runHook preInstall
    install -Dm755 mumble $out/bin/ruumble-client
    install -Dm644 ${root + "/server/web/public/icon.svg"} $out/share/icons/hicolor/scalable/apps/ruumble-client.svg
    mkdir -p $out/share/applications
    cat > $out/share/applications/ruumble-client.desktop <<EOF
    [Desktop Entry]
    Type=Application
    Name=Ruumble Client
    GenericName=Voice chat with the Ruumble building
    Comment=Mumble client with Ruumble built in
    Exec=ruumble-client %u
    Icon=ruumble-client
    Terminal=false
    Categories=Network;Chat;
    StartupWMClass=ruumble-client
    EOF
    runHook postInstall
  '';

  meta = {
    description = "Mumble client with the Ruumble office building as its window";
    homepage = "https://github.com/ruumble-project/ruumble-client";
    license = with lib.licenses; [
      bsd3
      lgpl3Only
    ];
    mainProgram = "ruumble-client";
    platforms = [
      "x86_64-linux"
      "aarch64-linux"
    ];
  };
}
