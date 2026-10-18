#!/bin/bash
# Runs inside the Sailfish OS Platform SDK container: package summary and
# Jolla's Harbour RPM validator (current rules from its repository).
set -euo pipefail
: "${SFOS_ARCH:?}"
shopt -s nullglob
rpms=(/workspace/RPMS/harbour-icqnina-[0-9]*."${SFOS_ARCH}".rpm)
[ ${#rpms[@]} -eq 1 ] || { echo "expected one app RPM, found: ${rpms[*]:-none}"; exit 1; }
RPM="${rpms[0]}"
echo "::group::Package"
rpm -qpi "$RPM"
echo "--- files"
rpm -qplv "$RPM"
echo "--- requires"
rpm -qpR "$RPM"
echo "--- provides"
rpm -qp --provides "$RPM"
echo "::endgroup::"
if git clone -q --depth 1 https://github.com/sailfishos/sdk-harbour-rpmvalidator.git /tmp/rpmvalidator; then
  /tmp/rpmvalidator/rpmvalidation.sh --no-color "$RPM"
else
  /usr/libexec/sdk-harbour-rpmvalidator/rpmvalidation.sh --no-color -g /usr/share/sdk-harbour-rpmvalidator "$RPM"
fi
