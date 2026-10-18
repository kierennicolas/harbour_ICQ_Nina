#!/bin/bash
# Live sign-on probe against NINA (nina.chat) with the account in the NINA_UIN
# and NINA_PASSWORD repository secrets. Output is redacted for public logs:
# other users' numbers are masked and message texts hidden.
#
# usage: ci/nina-probe.sh <path to oscarprobe>
set -uo pipefail
PROBE=$(readlink -f "$1")
: "${NINA_UIN:?NINA_UIN secret missing}" "${NINA_PASSWORD:?NINA_PASSWORD secret missing}"
export OSCAR_PASSWORD="$NINA_PASSWORD"
export QT_QPA_PLATFORM=minimal

run() {
  echo "== oscarprobe $*"
  timeout 120 "$PROBE" --uin "$NINA_UIN" --redact --trace "$@" 2>&1 | grep -v XDG_RUNTIME_DIR
  local rc=${PIPESTATUS[0]}
  echo "exit=$rc"
  return $rc
}

TAG="oscarprobe self-test $(date -u +%H%M%S)"

# 1. Plain OSCAR, ICQ 6.5 identity, stay on long enough to see presence and
#    the contact list, and send ourselves a message (NINA allows several
#    sessions, so it may be echoed back).
run --host login.oscar.nina.chat --port 5190 --identity icq65 --multiconn 1 --wait 45 \
    --to "$NINA_UIN" --message "$TAG" --send-delay 5
first=$?

# NINA, like the original servers, refuses sign-ons that come too quickly.
sleep 60

# 2. The same over SSL/TLS.
run --host slogin.oscar.nina.chat --port 5190 --tls --identity icq65 --multiconn 1 --wait 30
second=$?

echo "nina probe: plain=$first tls=$second"
exit $first
