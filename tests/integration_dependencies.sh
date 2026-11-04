#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
mkdir -p logs
: > logs/supervisor.log
TMP_CONFIG="$(mktemp)"
TMP_LOG="$(mktemp)"
cleanup() { rm -f "$TMP_CONFIG" "$TMP_LOG"; }
trap cleanup EXIT

cat > "$TMP_CONFIG" <<'EOF'
# Dependency order is intentionally reverse-listed.
app|1|1|worker|/bin/sleep 20
worker|1|1|base|/bin/sleep 20
base|1|1|-|/bin/sleep 20
EOF

./process_supervisor "$TMP_CONFIG" > /tmp/adas_supervisor_integration.out 2>&1 &
SUP_PID=$!
sleep 3
kill -TERM "$SUP_PID"
wait "$SUP_PID" || true

grep -q "Started base" logs/supervisor.log
grep -q "Started worker" logs/supervisor.log
grep -q "Started app" logs/supervisor.log
BASE_LINE="$(grep -n "Started base" logs/supervisor.log | tail -1 | cut -d: -f1)"
WORKER_LINE="$(grep -n "Started worker" logs/supervisor.log | tail -1 | cut -d: -f1)"
APP_LINE="$(grep -n "Started app" logs/supervisor.log | tail -1 | cut -d: -f1)"
test "$BASE_LINE" -lt "$WORKER_LINE"
test "$WORKER_LINE" -lt "$APP_LINE"
echo "Integration test passed: dependencies started in order."
