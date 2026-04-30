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
grep -Eq "export jsonl: written|export session jsonl: written" build/test/validate-tab-autocomplete.log || { echo "tab autocomplete failed for export jsonl" >&2; exit 1; }

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
grep -q "Audit journal TXT/JSONL: OK" build/storage/vita/export/reports/self-test.txt || { echo "self-test.txt missing separated journal status" >&2; exit 1; }
grep -q "Audit SQLite: OK" build/storage/vita/export/reports/self-test.txt || { echo "self-test.txt missing separated sqlite status for hosted pass case" >&2; exit 1; }
grep -q '"boot_id"' build/storage/vita/export/reports/last-session.jsonl || { echo "jsonl missing boot_id key" >&2; exit 1; }
grep -q '"event_type"' build/storage/vita/audit/sessions/session-000001.jsonl || { echo "jsonl missing event_type key" >&2; exit 1; }
while IFS= read -r line; do
  [[ "$line" == \{* ]] || { echo "jsonl line does not start with {" >&2; exit 1; }
  [[ "$line" == *\} ]] || { echo "jsonl line does not end with }" >&2; exit 1; }
  [[ "$line" == *'"event_type"'* ]] || { echo "jsonl line missing event_type" >&2; exit 1; }
  [[ "$line" == *'"seq"'* ]] || { echo "jsonl line missing seq" >&2; exit 1; }
done < build/storage/vita/audit/sessions/session-000001.jsonl
cat build/storage/vita/audit/sessions/session-000001*.jsonl | grep -q '"event_type":"session_end"' || cat build/storage/vita/audit/sessions/session-000001*.jsonl | grep -q '"event_type":"transcript_truncated"' || { echo "transcript missing session_end/truncated marker" >&2; exit 1; }
grep -q '"storage_state":"verified"' build/storage/vita/export/reports/diagnostic-bundle.jsonl || { echo "diagnostic jsonl did not keep verified storage state" >&2; exit 1; }

{
  printf 'help\n'
  i=0
  while [[ $i -lt 260 ]]; do
    printf 'storage status\njournal summary\n'
    i=$((i+1))
  done
  printf 'export jsonl\n'
  printf 'export session jsonl\n'
  printf 'shutdown\n'
} | ./build/hosted/vitaos-hosted > build/test/validate-transcript-long.log 2>&1
LONG_BASE=
LONG_PART2=
LONG_GLOB=
LONG_PART_COUNT=0
while IFS= read -r base_jsonl; do
  base_no_ext=${base_jsonl%.jsonl}
  candidate_part2="${base_no_ext}.part-0002.jsonl"
  [[ -f "$candidate_part2" ]] || continue
  candidate_part_count=$(find build/storage/vita/audit/sessions -maxdepth 1 -type f -name "$(basename "$base_no_ext").part-*.jsonl" | wc -l)
  if [[ "$candidate_part_count" -gt "$LONG_PART_COUNT" ]]; then
    LONG_BASE="$base_jsonl"
    LONG_PART2="$candidate_part2"
    LONG_GLOB="${base_no_ext}"'*.jsonl'
    LONG_PART_COUNT=$candidate_part_count
  fi
done < <(find build/storage/vita/audit/sessions -maxdepth 1 -type f -name 'session-*.jsonl' ! -name 'session-*.part-*.jsonl' | sort)
[[ -f "$LONG_BASE" ]] || { echo "missing long transcript base jsonl" >&2; exit 1; }
[[ -f "$LONG_PART2" ]] || { echo "missing long transcript part-0002 jsonl" >&2; exit 1; }
while IFS= read -r jf; do
  while IFS= read -r line; do
    [[ "$line" == \{* ]] || { echo "jsonl line does not start with { in $jf" >&2; exit 1; }
    [[ "$line" == *\} ]] || { echo "jsonl line does not end with } in $jf" >&2; exit 1; }
    [[ "$line" == *'"event_type"'* ]] || { echo "jsonl line missing event_type in $jf" >&2; exit 1; }
    [[ "$line" == *'"seq"'* ]] || { echo "jsonl line missing seq in $jf" >&2; exit 1; }
  done < "$jf"
done < <(find build/storage/vita/audit/sessions -maxdepth 1 -type f \( -name 'session-*.jsonl' -o -name 'session-*.part-*.jsonl' \) | sort)
grep -q 'Estado de almacenamiento / Storage status:' "$LONG_BASE" "${LONG_BASE%.jsonl}".part-*.jsonl || { echo "missing storage status output after rotation" >&2; exit 1; }
grep -q 'Resumen del journal / Journal summary:' "$LONG_BASE" "${LONG_BASE%.jsonl}".part-*.jsonl || { echo "missing journal summary output after rotation" >&2; exit 1; }
grep -q '"event_type":"session_end"' "$LONG_BASE" "${LONG_BASE%.jsonl}".part-*.jsonl || { echo "missing session_end in rotated jsonl parts" >&2; exit 1; }
grep -q '"event_type":"transcript_rotated"' "$LONG_BASE" "${LONG_BASE%.jsonl}".part-*.jsonl || { echo "missing transcript_rotated event" >&2; exit 1; }
grep -q '"event_type":"transcript_part_start"' "$LONG_BASE" "${LONG_BASE%.jsonl}".part-*.jsonl || { echo "missing transcript_part_start event" >&2; exit 1; }
! grep -q '"event_type":"transcript_truncated"' "$LONG_BASE" "${LONG_BASE%.jsonl}".part-*.jsonl || { echo "unexpected transcript_truncated after successful rotation" >&2; exit 1; }
grep -q 'transcript_jsonl_part_count' build/storage/vita/export/reports/last-session.jsonl || { echo "missing transcript_jsonl_part_count in jsonl report metadata" >&2; exit 1; }
grep -q "/vita/audit/sessions/$(basename "$LONG_PART2")" build/storage/vita/export/reports/last-session.jsonl || { echo "jsonl report metadata missing rotated part path" >&2; exit 1; }
prev_seq=-1
while IFS= read -r line; do
  seq=$(printf '%s\n' "$line" | sed -n 's/.*"seq":\([0-9][0-9]*\).*/\1/p')
  [[ -n "$seq" ]] || { echo "failed to parse seq in rotated transcript line" >&2; exit 1; }
  if [[ "$prev_seq" -ge 0 && "$seq" -lt "$prev_seq" ]]; then
    echo "rotated transcript seq decreased: $seq after $prev_seq" >&2
    exit 1
  fi
  prev_seq=$seq
done < <(cat "$LONG_BASE" "${LONG_BASE%.jsonl}".part-*.jsonl)

cat <<'CMDS' | ./build/hosted/vitaos-hosted > build/test/validate-previous-session.log 2>&1
status
shutdown
CMDS
LONG_SESSION_NUM=$(basename "$LONG_BASE" | sed -n 's/^session-\([0-9][0-9]*\)\.jsonl$/\1/p')
[[ -n "$LONG_SESSION_NUM" ]] || { echo "failed to parse long transcript session number" >&2; exit 1; }
NEXT_SESSION_NUM=$(printf "%06d" $((10#$LONG_SESSION_NUM + 1)))
NEXT_SESSION_BASE="build/storage/vita/audit/sessions/session-${NEXT_SESSION_NUM}.jsonl"
[[ -f "$NEXT_SESSION_BASE" ]] || { echo "missing session-${NEXT_SESSION_NUM} transcript jsonl" >&2; exit 1; }
! grep -q '"event_type":"previous_session_unclean"' "$NEXT_SESSION_BASE" "${NEXT_SESSION_BASE%.jsonl}".part-*.jsonl 2>/dev/null || { echo "false previous_session_unclean for rotated clean previous session" >&2; exit 1; }

if find build/storage/vita/export/reports -type f \( -name '*.txt' -o -name '*.jsonl' \) \
  -exec grep -n $'\033\\[' {} + >/dev/null; then
  echo "ansi escape codes found in export reports" >&2
  exit 1
fi
if find build/storage/vita/audit/sessions -maxdepth 1 -type f \( -name 'session-*.txt' -o -name 'session-*.jsonl' -o -name 'session-*.part-*.jsonl' \) \
  -exec grep -n $'\033\\[' {} + >/dev/null; then
  echo "ansi escape codes found in session artifacts" >&2
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

if grep -q "/vita/export/audit/audit-verify.txt" build/storage/vita/export/export-index.txt; then
  [[ -f build/storage/vita/export/audit/audit-verify.txt ]] || { echo "export-index lists missing audit-verify.txt" >&2; exit 1; }
fi
if grep -q "/vita/export/audit/current-session-events.txt" build/storage/vita/export/export-index.txt; then
  [[ -f build/storage/vita/export/audit/current-session-events.txt ]] || { echo "export-index lists missing current-session-events.txt" >&2; exit 1; }
fi

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
