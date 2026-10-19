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

# 3. Contact list editing: add our own number to our list and remove it
#    again (nobody else is involved), recording NINA's answers.
sleep 60
run --host login.oscar.nina.chat --port 5190 --identity icq65 --multiconn 1 --wait 40 \
    --add-contact "$NINA_UIN" --remove-contact "$NINA_UIN" --icon-server --icon-roundtrip --profile-dump

# 4. NINA's rendezvous proxy (ARS), which file transfers from a phone rely
#    on. NINA hands a file offer to yourself back only to the session that
#    sent it, so a whole transfer needs two accounts; this checks that the
#    proxy accepts us and assigns a relay port (the offer then times out).
echo "== rendezvous proxy candidates"
proxy=""
for h in ars.oscar.nina.bz ars.oscar.nina.chat ars.nina.chat ars.nina.bz; do
  addr=$(getent ahostsv4 "$h" | awk 'NR==1 {print $1}')
  echo "DNS $h ${addr:-unresolved}"
  [ -z "$proxy" ] && [ -n "$addr" ] && proxy=$h
done
echo "PROXY-HOST ${proxy:-none}"
proxy_rc=1
if [ -n "$proxy" ]; then
  sleep 60
  WORK=$(mktemp -d)
  head -c 50000 /dev/urandom > "$WORK/oscarprobe-ft.bin"
  out=$(run --host login.oscar.nina.chat --port 5190 --identity icq65 --multiconn 1 --wait 25 \
        --to "$NINA_UIN" --send-file "$WORK/oscarprobe-ft.bin" --send-delay 5 --ft-proxy --ft-proxy-host "$proxy")
  echo "$out"
  rm -rf "$WORK"
  if echo "$out" | grep -q 'TRACE got port'; then
    echo "FT-RESULT proxy handshake ok ($proxy)"; proxy_rc=0
  else
    echo "FT-RESULT proxy handshake failed ($proxy)"
  fi
else
  echo "FT-RESULT proxy skipped (no proxy host resolves)"
fi

# 5. Diagnostics requested with "Run workflow" (input nina_args).
if [ -n "${NINA_EXTRA:-}" ]; then
  read -r -a extra <<< "$NINA_EXTRA"
  sleep 60
  run --host login.oscar.nina.chat --port 5190 --identity icq65 --multiconn 1 --wait 30 "${extra[@]}"
fi

# (tools/ninacheck runs the app core's profile save against NINA; it writes
# to the account, so it is only run by hand. Profile writes are not part of
# the regular run: NINA stopped answering them for the test account after a
# series of test writes on 2026-10-19.)

echo "nina probe: plain=$first tls=$second ft_proxy=$proxy_rc"
exit $first
