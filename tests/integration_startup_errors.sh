#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
mkdir -p logs
TMP_CONFIG="$(mktemp)"
TMP_OUTPUT="$(mktemp)"
cleanup() { rm -f "$TMP_CONFIG" "$TMP_OUTPUT"; }
trap cleanup EXIT

if ./process_supervisor --invalid-option >"$TMP_OUTPUT" 2>&1; then
    echo "FAIL: invalid command-line arguments were accepted" >&2
    exit 1
fi

if ./process_supervisor "/tmp/adas_missing_config_$$.conf" >"$TMP_OUTPUT" 2>&1; then
    echo "FAIL: missing configuration file was accepted" >&2
    exit 1
fi

printf 'broken|not-a-number|1|-|/bin/sleep 1\n' > "$TMP_CONFIG"
if ./process_supervisor "$TMP_CONFIG" >"$TMP_OUTPUT" 2>&1; then
    echo "FAIL: invalid configuration was accepted" >&2
    exit 1
fi

echo "Integration test passed: invalid arguments and configuration failures."
