#!/usr/bin/env bash
set -euo pipefail

SUCCESS_LOG=${1:-build/test/boot-auto-storage-flow.log}
FAIL_LOG=${2:-build/test/boot-storage-failure.log}

mkdir -p build/test

rm -rf build/storage
cat <<'EOS' | ./build/hosted/vitaos-hosted > "$SUCCESS_LOG" 2>&1
status
audit status
storage status
storage last-error
storage check
journal status
journal summary
note usb-test.txt
notes list
storage read /vita/notes/usb-test.txt
export session
export jsonl
diagnostic
export index
selftest
shutdown
EOS

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

grep -q "storage bootstrap: verified" "$SUCCESS_LOG" || { echo "log missing storage bootstrap verified" >&2; exit 1; }
grep -q "storage: verified writable" "$SUCCESS_LOG" || { echo "log missing storage verified writable" >&2; exit 1; }
grep -q "journal: active" "$SUCCESS_LOG" || { echo "log missing journal active" >&2; exit 1; }
grep -q "note: quick-create verified" "$SUCCESS_LOG" || { echo "log missing note quick-create verified" >&2; exit 1; }
grep -q "export session: written" "$SUCCESS_LOG" || { echo "log missing export session written" >&2; exit 1; }
grep -q "export jsonl: written" "$SUCCESS_LOG" || { echo "log missing export jsonl written" >&2; exit 1; }
if grep -q "run storage repair" "$SUCCESS_LOG"; then
  echo "log incorrectly suggests storage repair prerequisite" >&2
  exit 1
fi

rm -rf build/storage
printf 'not-a-dir' > build/storage
cat <<'EOS' | ./build/hosted/vitaos-hosted > "$FAIL_LOG" 2>&1
status
audit status
storage status
storage last-error
journal status
note fail.txt
export session
export jsonl
diagnostic
selftest
shutdown
EOS
rm -f build/storage
mkdir -p build/storage

grep -q "storage bootstrap: failed" "$FAIL_LOG" || { echo "failure log missing storage bootstrap failed" >&2; exit 1; }
grep -q "storage: unavailable or unverified" "$FAIL_LOG" || { echo "failure log missing unverified storage" >&2; exit 1; }
grep -q "journal: inactive" "$FAIL_LOG" || { echo "failure log missing journal inactive" >&2; exit 1; }
grep -q "storage last-error" "$FAIL_LOG" || { echo "failure log missing storage last-error output" >&2; exit 1; }
grep -q "bootstrap failed:" "$FAIL_LOG" || { echo "failure log missing real bootstrap error cause" >&2; exit 1; }

if grep -q "storage: verified writable" "$FAIL_LOG"; then
  echo "failure log has false positive storage verified writable" >&2
  exit 1
fi
if grep -q "export session: written" "$FAIL_LOG"; then
  echo "failure log has false positive export session written" >&2
  exit 1
fi
if grep -q "export jsonl: written" "$FAIL_LOG"; then
  echo "failure log has false positive export jsonl written" >&2
  exit 1
fi
if grep -q "diagnostic: written" "$FAIL_LOG"; then
  echo "failure log has false positive diagnostic written" >&2
  exit 1
fi
if grep -q "self_test: PASS" "$FAIL_LOG"; then
  echo "failure log has false positive self_test PASS" >&2
  exit 1
fi

echo "validate-storage-persistence: PASS"
