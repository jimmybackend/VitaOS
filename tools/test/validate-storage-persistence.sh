#!/usr/bin/env bash
set -euo pipefail

LOG_SUCCESS=${1:-build/test/validate-storage-persistence.log}
LOG_FAILURE=${2:-build/test/validate-storage-failure.log}

mkdir -p build/test

if [[ ! -x build/hosted/vitaos-hosted ]]; then
  make hosted
fi

rm -rf build/storage
mkdir -p build/test

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
export session jsonl
diagnostic
export index
selftest
shutdown
CMDS

printf 'storage sta\t\njournal sum\t\nexport j\t\nshutdown\n' | ./build/hosted/vitaos-hosted > build/test/validate-tab-autocomplete.log 2>&1
grep -q "Estado de almacenamiento / Storage status:" build/test/validate-tab-autocomplete.log || { echo "tab autocomplete failed for storage status" >&2; exit 1; }
grep -q "Resumen del journal / Journal summary:" build/test/validate-tab-autocomplete.log || { echo "tab autocomplete failed for journal summary" >&2; exit 1; }
grep -q "export jsonl: written" build/test/validate-tab-autocomplete.log || { echo "tab autocomplete failed for export jsonl" >&2; exit 1; }

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
grep -q '\"event_type\":\"system_output\".*Estado de almacenamiento / Storage status:' build/storage/vita/audit/sessions/session-000001.jsonl || { echo "missing storage status system_output in transcript jsonl" >&2; exit 1; }
grep -q '\"event_type\":\"system_output\".*Resumen del journal / Journal summary:' build/storage/vita/audit/sessions/session-000001.jsonl || { echo "missing journal summary system_output in transcript jsonl" >&2; exit 1; }
grep -q "command_executed" build/storage/vita/audit/sessions/session-000001.jsonl || { echo "missing command_executed in transcript jsonl" >&2; exit 1; }
grep -q '"arch":"' build/storage/vita/audit/sessions/session-000001.jsonl || { echo "missing arch in transcript jsonl" >&2; exit 1; }
grep -q '"firmware":"' build/storage/vita/audit/sessions/session-000001.jsonl || { echo "missing firmware in transcript jsonl" >&2; exit 1; }
grep -q '"storage_state":"' build/storage/vita/audit/sessions/session-000001.jsonl || { echo "missing storage_state in transcript jsonl" >&2; exit 1; }
grep -q '"storage_backend":"' build/storage/vita/audit/sessions/session-000001.jsonl || { echo "missing storage_backend in transcript jsonl" >&2; exit 1; }
! grep -q "Wi-Fi OK\\|network OK" build/storage/vita/audit/sessions/session-000001.jsonl || { echo "transcript jsonl contains false network readiness claim" >&2; exit 1; }
grep -q "Resumen de sesion VitaOS" build/storage/vita/export/reports/last-session.txt || { echo "last-session.txt missing spanish summary heading" >&2; exit 1; }
grep -q "Diagnostico" build/storage/vita/export/reports/diagnostic-bundle.txt || { echo "diagnostic-bundle.txt missing diagnostico heading" >&2; exit 1; }
grep -q "Resultado" build/storage/vita/export/reports/self-test.txt || { echo "self-test.txt missing resultado heading" >&2; exit 1; }
grep -q '"boot_id"' build/storage/vita/export/reports/last-session.jsonl || { echo "jsonl missing boot_id key" >&2; exit 1; }
grep -q '"event_type"' build/storage/vita/audit/sessions/session-000001.jsonl || { echo "jsonl missing event_type key" >&2; exit 1; }
while IFS= read -r line; do
  [[ "$line" == \{* ]] || { echo "jsonl line does not start with {" >&2; exit 1; }
  [[ "$line" == *\} ]] || { echo "jsonl line does not end with }" >&2; exit 1; }
  [[ "$line" == *'"event_type"'* ]] || { echo "jsonl line missing event_type" >&2; exit 1; }
  [[ "$line" == *'"seq"'* ]] || { echo "jsonl line missing seq" >&2; exit 1; }
done < build/storage/vita/audit/sessions/session-000001.jsonl
grep -q "session_end" build/storage/vita/audit/sessions/session-000001.jsonl || grep -q "transcript_truncated" build/storage/vita/audit/sessions/session-000001.jsonl || { echo "transcript missing session_end/truncated marker" >&2; exit 1; }
grep -q '"storage_state":"verified"' build/storage/vita/export/reports/diagnostic-bundle.jsonl || { echo "diagnostic jsonl did not keep verified storage state" >&2; exit 1; }

cat <<'CMDS' | ./build/hosted/vitaos-hosted > build/test/validate-transcript-long.log 2>&1
note long.txt
aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa
.save
shutdown
CMDS
LONG_JSONL=build/storage/vita/audit/sessions/session-000002.jsonl
[[ -f "$LONG_JSONL" ]] || { echo "missing long transcript jsonl" >&2; exit 1; }
tail -n 1 "$LONG_JSONL" | grep -Eq '"event_type":"(session_end|transcript_truncated)"' || { echo "long transcript ended with invalid event" >&2; exit 1; }
if find build/storage/vita/export/reports -type f \( -name '*.txt' -o -name '*.jsonl' \) \
  -exec grep -n $'\033\\[' {} + >/dev/null; then
  echo "ansi escape codes found in export reports" >&2
  exit 1
fi

grep -q "storage bootstrap: verified" "$LOG_SUCCESS" || { echo "log missing storage bootstrap verified" >&2; exit 1; }
grep -q "storage: verified writable" "$LOG_SUCCESS" || { echo "log missing storage writable verification" >&2; exit 1; }
grep -q "journal: activo / active" "$LOG_SUCCESS" || { echo "log missing journal active" >&2; exit 1; }

for bad in \
  "storage bootstrap: failed" \
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
grep -qi "failed" "$LOG_FAILURE" || { echo "failure log missing generic failed marker" >&2; exit 1; }
grep -qi "storage last error\|storage_last_error\|last-error\|error" "$LOG_FAILURE" || { echo "failure log missing storage error cause" >&2; exit 1; }

# Guard against transcript_error audit spam during persistent storage failure loops.
JOURNAL_JSONL=build/storage/vita/audit/session-journal.jsonl
if [[ -f "$JOURNAL_JSONL" ]]; then
  transcript_error_count=$(grep -c '"event_type":"transcript_error"' "$JOURNAL_JSONL" || true)
  [[ "$transcript_error_count" -le 1 ]] || {
    echo "transcript_error repeated unexpectedly ($transcript_error_count)" >&2
    exit 1
  }
fi

rm -f build/storage
mkdir -p build/storage

echo "validate-storage-persistence: ok"
