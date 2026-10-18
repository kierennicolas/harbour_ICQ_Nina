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
#    the contact list, send ourselves a message (NINA allows several
#    sessions, so it may be echoed back), and try the ICQ directory on our
#    own number (read-only).
run --host login.oscar.nina.chat --port 5190 --identity icq65 --multiconn 1 --wait 45 \
    --to "$NINA_UIN" --message "$TAG" --send-delay 5 --info "$NINA_UIN" --search-uin "$NINA_UIN"
first=$?

# NINA, like the original servers, refuses sign-ons that come too quickly.
sleep 60

# 2. The same over SSL/TLS.
run --host slogin.oscar.nina.chat --port 5190 --tls --identity icq65 --multiconn 1 --wait 30
second=$?

# 3. File transfer to ourselves between two sessions of the test account
#    (nobody else is involved): first through NINA's rendezvous proxy, which
#    is what works from a phone, then directly (both sessions are on this
#    runner).
echo "== rendezvous proxy candidates"
proxy=""
for h in ars.oscar.nina.bz ars.oscar.nina.chat ars.nina.chat ars.nina.bz; do
  addr=$(getent ahostsv4 "$h" | awk 'NR==1 {print $1}')
  echo "DNS $h ${addr:-unresolved}"
  [ -z "$proxy" ] && [ -n "$addr" ] && proxy=$h
done
echo "PROXY-HOST ${proxy:-none}"

WORK=$(mktemp -d)
head -c 200000 /dev/urandom > "$WORK/oscarprobe-ft.bin"
ft() { # label, extra sender args...
  local label=$1; shift
  rm -rf "$WORK/recv"
  timeout 150 "$PROBE" --uin "$NINA_UIN" --redact --trace --host login.oscar.nina.chat --multiconn 1 --wait 110 \
      --receive-dir "$WORK/recv" --expect-file oscarprobe-ft.bin "$@" > "$WORK/receiver.log" 2>&1 &
  local receiver=$!
  sleep 30
  echo "== file transfer ($label): sender"
  timeout 120 "$PROBE" --uin "$NINA_UIN" --redact --trace --host login.oscar.nina.chat --multiconn 1 --wait 70 \
      --to "$NINA_UIN" --send-file "$WORK/oscarprobe-ft.bin" --send-delay 8 --ft-timeout 20 "$@" 2>&1 \
      | grep -v -E 'XDG_RUNTIME_DIR|FT-PROGRESS'
  wait $receiver; local rc=$?
  echo "== file transfer ($label): receiver"
  grep -v -E 'XDG_RUNTIME_DIR|FT-PROGRESS' "$WORK/receiver.log"
  if [ $rc -eq 0 ] && cmp -s "$WORK/oscarprobe-ft.bin" "$WORK/recv/oscarprobe-ft.bin"; then
    echo "FT-RESULT $label ok"; return 0
  fi
  echo "FT-RESULT $label failed (receiver exit $rc)"; return 1
}
sleep 60
proxy_rc=1
if [ -n "$proxy" ]; then
  ft proxy --ft-proxy --ft-proxy-host "$proxy"; proxy_rc=$?
else
  echo "FT-RESULT proxy skipped (no proxy host resolves)"
fi
sleep 60
ft direct; direct_rc=$?
rm -rf "$WORK"

echo "nina probe: plain=$first tls=$second ft_proxy=$proxy_rc ft_direct=$direct_rc"
exit $first
