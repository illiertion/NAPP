#!/usr/bin/env bash
# Phase 5 smoke test: start two apps in two runtime host processes and verify
# that both stay alive, get separate CEF profiles, and log no fatal errors.
set -u

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
HOST="$ROOT/build/cef/runtime-stage/napp-runtime-host.exe"
SMOKE="$ROOT/build/smoke"

if [ ! -f "$HOST" ]; then
    echo "FAIL: runtime host not built ($HOST). Run scripts/build-cef.bat first."
    exit 1
fi

rm -rf "$SMOKE"
mkdir -p "$SMOKE/appA" "$SMOKE/appB"

python - "$ROOT/tests/fixtures/valid.ipkg" "$SMOKE/appA" <<'PY'
import sys, zipfile
zipfile.ZipFile(sys.argv[1]).extractall(sys.argv[2])
PY
python - "$ROOT/tests/fixtures/other.ipkg" "$SMOKE/appB" <<'PY'
import sys, zipfile
zipfile.ZipFile(sys.argv[1]).extractall(sys.argv[2])
PY

"$HOST" --app-id com.illiertion.fixture --app-name "Fixture App" \
    --app-root "$SMOKE/appA" --entry app/index.html --data-dir "$SMOKE/dataA" &
PID_A=$!

"$HOST" --app-id com.illiertion.other --app-name "Other App" \
    --app-root "$SMOKE/appB" --entry app/index.html --data-dir "$SMOKE/dataB" &
PID_B=$!

echo "host A pid=$PID_A, host B pid=$PID_B"
sleep 12

STATUS=0
if kill -0 "$PID_A" 2>/dev/null; then echo "PASS: app A process alive"; else echo "FAIL: app A exited"; STATUS=1; fi
if kill -0 "$PID_B" 2>/dev/null; then echo "PASS: app B process alive"; else echo "FAIL: app B exited"; STATUS=1; fi

for DIR in dataA dataB; do
    if [ -d "$SMOKE/$DIR/web" ] && [ -d "$SMOKE/$DIR/Default" ]; then
        echo "PASS: $DIR has its own Chromium profile (web/ + Default/)"
    else
        echo "FAIL: $DIR Chromium profile missing"
        STATUS=1
    fi
done

if [ -f "$SMOKE/dataA/Local State" ] && [ -f "$SMOKE/dataB/Local State" ] \
   && ! cmp -s "$SMOKE/dataA/Local State" "$SMOKE/dataB/Local State"; then
    echo "PASS: profile state files are independent"
else
    echo "FAIL: profile state files missing or shared"
    STATUS=1
fi

for PAIR in "dataA:com.illiertion.fixture" "dataB:com.illiertion.other"; do
    DIR="${PAIR%%:*}"
    APP="${PAIR##*:}"
    LOG="$SMOKE/$DIR/logs/runtime.log"
    if [ ! -f "$LOG" ]; then
        echo "FAIL: log missing: $LOG"
        STATUS=1
        continue
    fi
    if grep -qiE "CefInitialize failed|invalid runtime arguments|load error" "$LOG"; then
        echo "FAIL: errors in $LOG"
        grep -iE "CefInitialize failed|invalid runtime arguments|load error" "$LOG" | head -5
        STATUS=1
    fi
    if grep -q "browser ready for $APP" "$LOG"; then
        echo "PASS: $APP created a browser instance"
    else
        echo "FAIL: $APP never created a browser instance"
        STATUS=1
    fi
    if grep -q "served app/index.html" "$LOG"; then
        echo "PASS: $APP entry document served over napp://"
    else
        echo "FAIL: $APP entry document was not served"
        STATUS=1
    fi
done

kill "$PID_A" "$PID_B" 2>/dev/null
wait "$PID_A" 2>/dev/null
wait "$PID_B" 2>/dev/null

echo "smoke result: $([ $STATUS -eq 0 ] && echo PASS || echo FAIL)"
exit $STATUS
