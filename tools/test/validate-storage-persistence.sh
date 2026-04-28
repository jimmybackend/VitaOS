#!/usr/bin/env bash
set -euo pipefail

LOG=${1:-build/test/boot-auto-storage-flow.log}

required_files=(
  build/storage/vita/tmp/boot-storage-verify.txt
  build/storage/vita/audit/session-journal.txt
  build/storage/vita/audit/session-journal.jsonl
  build/storage/vita/notes/usb-test.txt
  build/storage/vita/export/reports/last-session.txt
  build/storage/vita/export/reports/last-session.jsonl
  build/storage/vita/export/reports/diagnostic-bundle.txt
  build/storage/vita/export/export-index.txt
  build/storage/vita/export/reports/self-test.txt
)

for f in "${required_files[@]}"; do
  [[ -f "$f" ]] || { echo "missing required file: $f" >&2; exit 1; }
done

grep -q "storage bootstrap: verified" "$LOG" || { echo "log missing storage bootstrap verified" >&2; exit 1; }
grep -q "Persistent journal: READY" "$LOG" || { echo "log missing journal ready" >&2; exit 1; }
grep -q "note: quick-create verified" "$LOG" || { echo "log missing note quick-create verified" >&2; exit 1; }
grep -q "export session: written" "$LOG" || { echo "log missing export session written" >&2; exit 1; }
grep -q "export jsonl: written" "$LOG" || { echo "log missing export jsonl written" >&2; exit 1; }

echo "validate-storage-persistence: ok"
