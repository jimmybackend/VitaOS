#!/usr/bin/env bash
set -euo pipefail

ROOT=$(pwd)
MAKEFILE="$ROOT/Makefile"
COMMAND_C="$ROOT/kernel/command_core.c"
SELFTEST_C="$ROOT/kernel/selftest.c"
SELFTEST_H="$ROOT/include/vita/selftest.h"
DOC="$ROOT/docs/boot/self-test.md"

if [[ ! -f "$MAKEFILE" || ! -f "$COMMAND_C" || ! -f "$SELFTEST_C" || ! -f "$SELFTEST_H" ]]; then
  echo "[verify-v31] ERROR: missing v31 files; run apply-selftest-v31.sh first" >&2
  exit 1
fi

required=(
  "vita_selftest_run"
  "SELFTEST_TEXT_PATH"
  "SELFTEST_JSONL_PATH"
  "selftest"
  "boot self-test"
  "kernel/selftest.c"
)

for sym in "${required[@]}"; do
  if ! grep -Rqs "$sym" "$MAKEFILE" "$COMMAND_C" "$SELFTEST_C" "$SELFTEST_H" "$DOC" 2>/dev/null; then
    echo "[verify-v31] ERROR: missing $sym" >&2
    exit 1
  fi
done

echo "[verify-v31] OK: boot self-test command patch present"
