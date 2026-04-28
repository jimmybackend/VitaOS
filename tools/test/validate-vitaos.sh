#!/usr/bin/env bash
set -euo pipefail

LOG=build/test/boot-auto-storage-flow.log

mkdir -p build/test

make clean
make hosted
make smoke-audit
make

printf '%s\n' \
  'storage status' \
  'storage last-error' \
  'storage check' \
  'journal status' \
  'journal summary' \
  'note usb-test.txt' \
  'auto bootstrap note line' \
  '.save' \
  '.exit' \
  'notes list' \
  'storage read /vita/notes/usb-test.txt' \
  'export session' \
  'export jsonl' \
  'diagnostic' \
  'export index' \
  'selftest' \
  'shutdown' \
  | ./build/hosted/vitaos-hosted > "$LOG" 2>&1

./tools/test/validate-storage-persistence.sh "$LOG"

echo "validate-vitaos: ok"
