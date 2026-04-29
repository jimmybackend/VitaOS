#!/usr/bin/env bash
set -euo pipefail

LOG_SUCCESS=${1:-build/test/validate-storage-persistence.log}
LOG_FAILURE=${2:-build/test/validate-storage-failure.log}

mkdir -p build/test

if [[ ! -x build/hosted/vitaos-hosted ]]; then
  make hosted
fi

rm -rf build/storage

cat <<'CMDS' | ./build/hosted/vitaos-hosted > "$LOG_SUCCESS" 2>&1
storage status
storage last-error
storage check
journal status
journal summary
note usb-test.txt
primera linea persistente
.save
.exit
notes list
storage read /vita/notes/usb-test.txt
export session
export jsonl
diagnostic
export index
selftest
shutdown
CMDS

required_files=(
  build/storage/vita/tmp/boot-storage-verify.txt
  build/storage/vita/audit/session-journal.txt
  build/storage/vita/audit/session-journal.jsonl
  build/storage/vita/audit/sessions/session-000001.txt
  build/storage/vita/audit/sessions/session-000001.jsonl
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

grep -q "session_start" build/storage/vita/audit/sessions/session-000001.jsonl || { echo "missing session_start in transcript jsonl" >&2; exit 1; }
grep -q "user_input" build/storage/vita/audit/sessions/session-000001.jsonl || { echo "missing user_input in transcript jsonl" >&2; exit 1; }
grep -q "system_output" build/storage/vita/audit/sessions/session-000001.jsonl || { echo "missing system_output in transcript jsonl" >&2; exit 1; }
grep -q "command_executed" build/storage/vita/audit/sessions/session-000001.jsonl || { echo "missing command_executed in transcript jsonl" >&2; exit 1; }

grep -q "storage bootstrap: verified" "$LOG_SUCCESS" || { echo "log missing storage bootstrap verified" >&2; exit 1; }
grep -q "storage: verified writable" "$LOG_SUCCESS" || { echo "log missing storage writable verification" >&2; exit 1; }
grep -q "journal: active" "$LOG_SUCCESS" || { echo "log missing journal active" >&2; exit 1; }

for bad in \
  "storage bootstrap: failed" \
  "journal: inactive" \
  "export session: failed" \
  "export jsonl: failed" \
  "diagnostic: failed" \
  "export index: failed" \
  "selftest: failed"; do
  ! grep -q "$bad" "$LOG_SUCCESS" || { echo "unexpected failure marker in success log: $bad" >&2; exit 1; }
done

rm -rf build/storage
printf 'not-a-dir' > build/storage

cat <<'CMDS' | ./build/hosted/vitaos-hosted > "$LOG_FAILURE" 2>&1
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
CMDS

for bad_positive in \
  "storage: verified writable" \
  "storage bootstrap: verified" \
  "journal: active" \
  "export session: written" \
  "export jsonl: written" \
  "diagnostic: written" \
  "selftest: pass" \
  "saved"; do
  ! grep -q "$bad_positive" "$LOG_FAILURE" || { echo "unexpected success marker in failure log: $bad_positive" >&2; exit 1; }
done

grep -q "storage bootstrap: failed" "$LOG_FAILURE" || { echo "failure log missing bootstrap failed marker" >&2; exit 1; }
grep -q "journal: inactive" "$LOG_FAILURE" || { echo "failure log missing journal inactive marker" >&2; exit 1; }
grep -qi "failed" "$LOG_FAILURE" || { echo "failure log missing generic failed marker" >&2; exit 1; }
grep -qi "storage last error\|storage_last_error\|last-error\|error" "$LOG_FAILURE" || { echo "failure log missing storage error cause" >&2; exit 1; }

rm -f build/storage
mkdir -p build/storage

echo "validate-storage-persistence: ok"
