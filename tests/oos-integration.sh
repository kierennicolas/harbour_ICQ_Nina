#!/bin/bash
# Integration tests for liboscar/oscarprobe against a local Open OSCAR Server
# (https://github.com/mk6i/open-oscar-server, MIT) with real password checks.
#
# usage: tests/oos-integration.sh <oscarprobe> <open_oscar_server> [tst_core]
set -uo pipefail
PROBE=$(readlink -f "$1")
OOS=$(readlink -f "$2")
CORE_TEST=${3:+$(readlink -f "$3")}
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

for u in 100001 100002 100003; do
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

HERE=$(dirname "$(readlink -f "$0")")
python3 "$HERE/fake-ars.py" 127.0.0.1 5191 > "$WORK/ars.log" 2>&1 &
ARS=$!
python3 "$HERE/fake-upload.py" 8099 > "$WORK/upload.log" 2>&1 &
UPLOAD=$!
trap 'kill $SERVER $ARS $UPLOAD 2>/dev/null; rm -rf "$WORK"' EXIT
sleep 1
head -c 300000 /dev/urandom > "$WORK/oscarprobe-ft.bin"
pause

echo "== file transfer, direct"
rm -rf "$WORK/recv"
probe 100002 pw100002 --wait 30 --receive-dir "$WORK/recv" --expect-file oscarprobe-ft.bin > "$WORK/ft-receiver.log" &
RECEIVER=$!
sleep 4
out=$(probe 100001 pw100001 --wait 20 --to 100002 --send-file "$WORK/oscarprobe-ft.bin" --send-delay 1); echo "$out" | grep -v FT-PROGRESS
wait $RECEIVER; rc=$?
grep -E 'FT-|RESULT' "$WORK/ft-receiver.log"
if [ $rc -eq 0 ] && cmp -s "$WORK/oscarprobe-ft.bin" "$WORK/recv/oscarprobe-ft.bin"; then
  pass "file received intact (direct)"; else fail "file transfer (direct)"; fi
pause

echo "== file transfer through a rendezvous proxy (tests/fake-ars.py)"
rm -rf "$WORK/recv"
probe 100002 pw100002 --wait 30 --receive-dir "$WORK/recv" --expect-file oscarprobe-ft.bin \
      --ft-proxy-host 127.0.0.1 --ft-proxy-port 5191 > "$WORK/ft-receiver.log" &
RECEIVER=$!
sleep 4
out=$(probe 100001 pw100001 --wait 20 --to 100002 --send-file "$WORK/oscarprobe-ft.bin" --send-delay 1 \
      --ft-proxy --ft-proxy-host 127.0.0.1 --ft-proxy-port 5191); echo "$out" | grep -v FT-PROGRESS
wait $RECEIVER; rc=$?
grep -E 'FT-|RESULT' "$WORK/ft-receiver.log"
cat "$WORK/ars.log"
if [ $rc -eq 0 ] && cmp -s "$WORK/oscarprobe-ft.bin" "$WORK/recv/oscarprobe-ft.bin" && grep -q 'connected' "$WORK/ars.log"; then
  pass "file received intact (proxy)"; else fail "file transfer (proxy)"; fi
pause

echo "== ICQ info and directory search"
out=$(probe 100001 pw100001 --wait 10 --info 100002 --search-uin 100002); echo "$out" | grep -E 'INFO|SEARCH'
echo "$out" | grep -q 'INFO-FULL 100002' && pass "full info" || fail "full info"
echo "$out" | grep -q 'SEARCH-RESULT uin=100002' && pass "search by number" || fail "search by number"

if [ -n "$CORE_TEST" ]; then
  pause; pause
  echo "== app core (AccountManager) against the server"
  if QT_QPA_PLATFORM=minimal OSCAR_TEST_HOST=127.0.0.1 OSCAR_TEST_ARS=127.0.0.1:5191 \
     ICQNINA_TEST_UPLOAD_ENDPOINT=http://127.0.0.1:8099/api.php ICQNINA_TEST_UPLOAD_HOST=files.test \
     timeout 420 "$CORE_TEST" 2>&1 | grep -v XDG_RUNTIME_DIR; then
    pass "app core tests"; else fail "app core tests"; fi
fi

echo
grep -E 'level=(ERROR|WARN)' "$LOG" | head -20
echo "integration: $fails failure(s)"
exit $fails
