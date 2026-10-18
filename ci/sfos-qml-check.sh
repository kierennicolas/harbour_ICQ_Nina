#!/bin/bash
# Runs inside the Sailfish OS Platform SDK container:
#  1. installs the RPM and its runtime dependencies into the SDK target, which
#     proves every Requires: resolves on Sailfish OS;
#  2. checks every theme icon the QML uses exists;
#  3. builds tests/qmlcheck and loads the installed QML with the real Silica,
#     Nemo.Notifications and Nemo.KeepAlive modules (ARM, under qemu).
set -uo pipefail
: "${SFOS_RELEASE:?}" "${SFOS_ARCH:?}"
TARGET="SailfishOS-${SFOS_RELEASE}-${SFOS_ARCH}"
SB2="sb2 -t $TARGET"
APPQML=/usr/share/harbour-icqnina/qml
shopt -s nullglob
rpms=(/workspace/RPMS/harbour-icqnina-[0-9]*."${SFOS_ARCH}".rpm)
[ ${#rpms[@]} -eq 1 ] || { echo "::error::expected one app RPM, found: ${rpms[*]:-none}"; exit 2; }
problems=0

echo "::group::Install the RPM and its dependencies into $TARGET"
$SB2 -m sdk-install -R zypper --non-interactive refresh >/dev/null 2>&1 || true
deps=$(rpm -qpR "${rpms[0]}" | awk '$1 !~ /^(rpmlib\(|\/|lib.*\.so)/ {print $1}' | sort -u)
echo "Requires (packages): $deps"
$SB2 -m sdk-install -R zypper --non-interactive install $deps nemo-qml-plugin-notifications-qt5 libkeepalive qt5-plugin-sqldriver-sqlite \
  || { echo "::error::Could not install the app's dependencies"; problems=$((problems + 1)); }
mkdir -p "$HOME/rpm-in" && cp "${rpms[0]}" "$HOME/rpm-in/"
if ! $SB2 -m sdk-install -R rpm -Uvh --force "$HOME/rpm-in/${rpms[0]##*/}"; then
  echo "::error::The RPM does not install on $TARGET"
  exit 1
fi
$SB2 rpm -q harbour-icqnina
$SB2 ls -la /usr/share/harbour-icqnina/lib
echo "::endgroup::"

echo "::group::Theme icons used by the app"
find_icon() { $SB2 /bin/sh -c "find /usr/share/themes -name '$1.png' -o -name '$1.svg' 2>/dev/null" | head -1; }
if [ -z "$(find_icon icon-m-back)" ]; then
  all=$($SB2 -m sdk-install -R zypper --non-interactive search 'sailfish-content-graphics*' 2>/dev/null \
        | awk -F'|' 'NR>2 {gsub(/ /,"",$2); print $2}' | sort -u)
  pkgs=$(echo "$all" | grep -E '^sailfish-content-graphics-default-z1\.0(-base)?$')
  [ -n "$pkgs" ] && $SB2 -m sdk-install -R zypper --non-interactive install $pkgs >/dev/null 2>&1
fi
if [ -z "$(find_icon icon-m-back)" ]; then
  echo "No theme icons in this SDK target; skipping."
else
  for icon in $(grep -rhoE 'image://theme/[A-Za-z0-9_-]+' /workspace/app/qml | sed 's#image://theme/##' | sort -u); do
    if [ -n "$(find_icon "$icon")" ]; then echo "OK      $icon"; else echo "MISSING $icon"; problems=$((problems + 1)); fi
  done
fi
echo "::endgroup::"

SRC="$HOME/qmlcheck-src"
rm -rf "$SRC" && mkdir -p "$SRC"
tar -C /workspace --exclude=./.git --exclude=./RPMS -cf - app tests | tar -C "$SRC" -xf -
mkdir -p "$SRC/liboscar" && tar -C /workspace -cf - liboscar | tar -C "$SRC" -xf -
mkdir -p "$SRC/tests/qmlcheck/build" && cd "$SRC/tests/qmlcheck/build"
echo "::group::Build qmlcheck"
$SB2 qmake ../qmlcheck.pro && $SB2 make -j"$(nproc)" || { echo "::error::qmlcheck build failed"; exit 2; }
echo "::endgroup::"

mkdir -p /tmp/xdg && chmod 700 /tmp/xdg
$SB2 /bin/sh -c 'XDG_RUNTIME_DIR=/tmp/xdg QT_QPA_PLATFORM=minimal QV4_FORCE_INTERPRETER=1 ./qmlcheck "$1"' _ "$APPQML" \
  || problems=$((problems + 1))
echo "qml check: $problems problem(s)"
[ $problems -eq 0 ]
