#!/bin/bash
# Integration tests for liboscar/oscarprobe against a local Open OSCAR Server
# (https://github.com/mk6i/open-oscar-server, MIT) with real password checks.
#
# usage: tests/oos-integration.sh <path to oscarprobe> <path to open_oscar_server>
set -uo pipefail
PROBE=$(readlink -f "$1")
OOS=$(readlink -f "$2")
WORK=$(mktemp -d)
LOG="$WORK/server.log"
fails=0
pass() { echo "PASS  $*"; }
fail() { echo "FAIL  $*"; fails=$((fails + 1)); }

cat > "$WORK/settings.env" <<CFG
export OSCAR_LISTENERS=LOCAL://127.0.0.1:5190
export OSCAR_ADVERTISED_LISTENERS_PLAIN=LOCAL://127.0.0.1:5190
export TOC_LISTENERS=127.0.0.1:9898
export API_LISTENER=127.0.0.1:8080
export DB_PATH=$WORK/oscar.sqlite
export DISABLE_AUTH=false
export LOG_LEVEL=info
export ICQ_LEGACY_ENABLED=false
CFG
(cd "$WORK" && set -a && . ./settings.env && set +a && exec "$OOS") > "$LOG" 2>&1 &
SERVER=$!
trap 'kill $SERVER 2>/dev/null; rm -rf "$WORK"' EXIT
for _ in $(seq 1 50); do curl -s -o /dev/null http://127.0.0.1:8080/user && break; sleep 0.2; done

for u in 100001 100002; do
  curl -s -o /dev/null -w "create $u: %{http_code}\n" -X POST http://127.0.0.1:8080/user \
       -H 'Content-Type: application/json' -d "{\"screen_name\":\"$u\",\"password\":\"pw$u\"}"
done

probe() { # account password args...
  local uin=$1 pw=$2; shift 2
  OSCAR_PASSWORD="$pw" QT_QPA_PLATFORM=minimal timeout 60 "$PROBE" --host 127.0.0.1 --port 5190 --uin "$uin" "$@" 2>&1 | grep -v XDG_RUNTIME_DIR
}
pause() { sleep 4; } # the server limits how fast one address may sign on

echo "== wrong password is rejected"
out=$(probe 100001 wrongpw1 --wait 5); echo "$out"
if echo "$out" | grep -q 'TASK-ERROR .*code=5 fatal=yes' && echo "$out" | grep -q 'RESULT login=failed'; then
  pass "wrong password rejected with error 5"; else fail "wrong password"; fi
pause

echo "== sign on with an ICQ number"
out=$(probe 100001 pw100001 --wait 3); echo "$out"
echo "$out" | grep -q 'LOGIN ok' && pass "ICQ sign-on" || fail "ICQ sign-on"
echo "$out" | grep -q 'CONTACT-LIST' && pass "server-side contact list received" || fail "contact list"
echo "$out" | grep -q 'OWN-INFO id=100001' && pass "own user info received" || fail "own info"
pause

echo "== Unicode message from 100001 to 100002, with delivery acknowledgement"
TEXT='Grüße aus Gibraltar ✓ Привет 😀'
probe 100002 pw100002 --wait 20 --expect-message "$TEXT" > "$WORK/receiver.log" &
RECEIVER=$!
sleep 3
out=$(probe 100001 pw100001 --wait 6 --to 100002 --message "$TEXT" --send-delay 1); echo "$out"
wait $RECEIVER; rc=$?
cat "$WORK/receiver.log"
[ $rc -eq 0 ] && grep -qF "text=\"$TEXT\"" "$WORK/receiver.log" && pass "message received intact" || fail "message received intact"
echo "$out" | grep -q 'ACK from=100002' && pass "delivery acknowledged" || fail "delivery acknowledgement"
pause

echo "== status: sign on as away"
out=$(probe 100002 pw100002 --wait 3 --status away); echo "$out"
echo "$out" | grep -q 'LOGIN ok' && pass "sign-on with away status" || fail "away sign-on"

echo
grep -E 'level=(ERROR|WARN)' "$LOG" | head -20
echo "integration: $fails failure(s)"
exit $fails
