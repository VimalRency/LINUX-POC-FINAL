#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

mkdir -p logs
: > logs/supervisor.log

TMP_CONFIG="$(mktemp)"
TMP_OUTPUT="$(mktemp)"

cleanup() {
    if [[ -n "${SUP_PID:-}" ]]; then
        kill -TERM "$SUP_PID" 2>/dev/null || true
        wait "$SUP_PID" 2>/dev/null || true
    fi
    rm -f "$TMP_CONFIG" "$TMP_OUTPUT"
}
trap cleanup EXIT

cat > "$TMP_CONFIG" <<'CONFIG'
signal_test|2|1|-|/bin/sleep 300
CONFIG

./process_supervisor "$TMP_CONFIG" >"$TMP_OUTPUT" 2>&1 &
SUP_PID=$!

get_child_pid() {
    local pid=""
    for _ in $(seq 1 50); do
        pid="$(sed -n 's/.*Started signal_test with PID //p' \
            logs/supervisor.log | tail -n 1)"
        if [[ -n "$pid" ]] && kill -0 "$pid" 2>/dev/null; then
            echo "$pid"
            return 0
        fi
        sleep 0.2
    done
    return 1
}

for attempt in 1 2 3; do
    CHILD_PID="$(get_child_pid)"
    echo "Sending SIGTERM to child PID $CHILD_PID"
    kill -TERM "$CHILD_PID"

    for _ in $(seq 1 50); do
        if grep -q "signal_test exhausted restart limit" logs/supervisor.log; then
            break
        fi

        START_COUNT="$(grep -c 'Started signal_test with PID' \
            logs/supervisor.log || true)"

        if [[ "$START_COUNT" -gt "$attempt" ]]; then
            break
        fi
        sleep 0.2
    done
done

for _ in $(seq 1 30); do
    grep -q "signal_test exhausted restart limit" logs/supervisor.log && break
    sleep 0.2
done

kill -TERM "$SUP_PID" 2>/dev/null || true
wait "$SUP_PID" 2>/dev/null || true
SUP_PID=""

START_COUNT="$(grep -c 'Started signal_test with PID' logs/supervisor.log || true)"
SIGNAL_COUNT="$(grep -c 'signal_test terminated by signal 15' logs/supervisor.log || true)"

test "$START_COUNT" -eq 3
test "$SIGNAL_COUNT" -eq 3
grep -q 'signal_test exhausted restart limit' logs/supervisor.log

echo "PASS: SIGTERM crash recovery and restart limit verified."
