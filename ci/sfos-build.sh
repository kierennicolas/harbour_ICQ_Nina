#!/bin/bash
# Runs inside the Sailfish OS Platform SDK container: builds liboscar and
# oscarprobe for $SFOS_ARCH with the target's Qt (5.6), then signs on to the
# Open OSCAR Server running on the CI host and exchanges a message, with the
# ARM binaries running under qemu.
set -uo pipefail
: "${SFOS_RELEASE:?}" "${SFOS_ARCH:?}"
TARGET="SailfishOS-${SFOS_RELEASE}-${SFOS_ARCH}"
SB2="sb2 -t $TARGET"

SRC="$HOME/src"
rm -rf "$SRC" && mkdir -p "$SRC"
tar -C /workspace --exclude=./.git -cf - . | tar -C "$SRC" -xf -
echo "::group::Build liboscar and oscarprobe for $TARGET"
mkdir -p "$SRC/build/liboscar" "$SRC/build/tools/oscarprobe"
(cd "$SRC/build/liboscar" && $SB2 qmake ../../liboscar/liboscar.pro && $SB2 make -j"$(nproc)") \
  || { echo "::error::liboscar build failed"; exit 1; }
(cd "$SRC/build/tools/oscarprobe" && $SB2 qmake ../../../tools/oscarprobe/oscarprobe.pro && $SB2 make -j"$(nproc)") \
  || { echo "::error::oscarprobe build failed"; exit 1; }
echo "::endgroup::"
cd "$SRC/build"
file liboscar/liboscar.so.1.0.0 tools/oscarprobe/oscarprobe

PROBE="$SRC/build/tools/oscarprobe/oscarprobe"
probe() { # uin password args...
  local uin=$1 pw=$2; shift 2
  $SB2 /bin/bash -c 'OSCAR_PASSWORD="$1" QT_QPA_PLATFORM=minimal LD_LIBRARY_PATH="$2" "$3" --host 127.0.0.1 --uin "$4" "${@:5}"' \
      _ "$pw" "$SRC/build/liboscar" "$PROBE" "$uin" "$@"
}

echo "== ARM build: sign on and send a message to a second account"
TEXT='Sailfish ARM build ✓ Grüße'
probe 100002 pw100002 --wait 40 --expect-message "$TEXT" > "$HOME/receiver.log" 2>&1 &
RECEIVER=$!
sleep 8
probe 100001 pw100001 --wait 15 --to 100002 --message "$TEXT" --send-delay 2 2>&1 | grep -v XDG
wait $RECEIVER; rc=$?
grep -v XDG "$HOME/receiver.log"
[ $rc -eq 0 ] && echo "ARM message test: PASS" || { echo "::error::ARM message test failed"; exit 1; }
