# Build environment for Ruumble Desk packages. BASE picks the distro (debian:12, ubuntu:24.04, ...).
# Only build tools and libraries; the sources come in read only when the container runs (packaging/in-container.sh).
ARG BASE=debian:12
FROM ${BASE}
ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
      build-essential cmake ninja-build pkgconf git ca-certificates curl file python3 \
      dpkg-dev fakeroot desktop-file-utils patchelf \
      qt6-base-dev qt6-tools-dev qt6-tools-dev-tools qt6-svg-dev qt6-l10n-tools \
      qt6-webengine-dev qt6-webengine-dev-tools qt6-webchannel-dev \
      libgl-dev libboost-dev libssl-dev libprotobuf-dev protobuf-compiler libprotoc-dev libcap-dev libxi-dev \
      libasound2-dev libpulse-dev libogg-dev libsndfile1-dev libopus-dev libpoco-dev nlohmann-json3-dev \
      libcli11-dev libutfcpp-dev libspdlog-dev libspeexdsp-dev \
      libqt6sql6-sqlite qt6-qpa-plugins \
 && rm -rf /var/lib/apt/lists/*
