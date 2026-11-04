#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
mkdir -p logs
: > logs/supervisor.log
TMP_CONFIG="$(mktemp)"
TMP_OUTPUT="$(mktemp)"
cleanup() { rm -f "$TMP_CONFIG" "$TMP_OUTPUT"; }
trap cleanup EXIT
cat > "$TMP_CONFIG" <<'CONFIG'
# crash has two retries after its initial launch
crash|2|1|-|/bin/false
clean|2|1|-|/bin/true
CONFIG
./process_supervisor "$TMP_CONFIG" >"$TMP_OUTPUT" 2>&1 &
SUP_PID=$!
for _ in $(seq 1 8); do
    if grep -q "crash exhausted restart limit" logs/supervisor.log; then break; fi
    sleep 1
done
kill -TERM "$SUP_PID" 2>/dev/null || true
wait "$SUP_PID" || true
START_COUNT="$(grep -c 'Started crash with PID' logs/supervisor.log || true)"
test "$START_COUNT" -eq 3
grep -q 'crash exhausted restart limit' logs/supervisor.log
if grep -q 'Restart scheduled for clean' logs/supervisor.log; then
    echo "FAIL: clean exit was incorrectly restarted" >&2
    exit 1
fi
echo "Integration test passed: bounded restart policy and clean-exit handling."
