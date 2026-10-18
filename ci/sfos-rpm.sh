#!/bin/bash
# Runs inside the Sailfish OS Platform SDK container: builds the app RPM for
# $SFOS_ARCH against SailfishOS-$SFOS_RELEASE and copies it to /workspace/RPMS.
set -euo pipefail
: "${SFOS_RELEASE:?}" "${SFOS_ARCH:?}"
TARGET="SailfishOS-${SFOS_RELEASE}-${SFOS_ARCH}"
BUILD="$HOME/rpmbuild-src"
rm -rf "$BUILD" && mkdir -p "$BUILD"
tar -C /workspace --exclude=./.git --exclude=./RPMS --exclude=./build -cf - . | tar -C "$BUILD" -xf -
cd "$BUILD"
mb2 -t "$TARGET" build
mkdir -p /workspace/RPMS
cp -v RPMS/*.rpm /workspace/RPMS/
