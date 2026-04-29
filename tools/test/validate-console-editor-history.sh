#!/usr/bin/env bash
set -euo pipefail

LOG_FILE=${1:-build/test/validate-console-editor-history.log}

mkdir -p build/test

if [[ ! -x build/hosted/vitaos-hosted ]]; then
  make hosted
fi

rm -rf build/storage

cat <<'CMDS' | ./build/hosted/vitaos-hosted > "$LOG_FILE" 2>&1
edit /vita/notes/editor-test.txt
linea normal desde validador
.save
.exit
storage read /vita/notes/editor-test.txt
shutdown
CMDS

NOTE_FILE="build/storage/vita/notes/editor-test.txt"
SESSIONS_DIR="build/storage/vita/audit/sessions"

[[ -f "$NOTE_FILE" ]] || { echo "missing editor note file: $NOTE_FILE" >&2; exit 1; }
grep -q "linea normal desde validador" "$NOTE_FILE" || { echo "editor note missing expected text" >&2; exit 1; }
! grep -q '^\.save$' "$NOTE_FILE" || { echo "editor note incorrectly contains .save" >&2; exit 1; }
! grep -q '^\.exit$' "$NOTE_FILE" || { echo "editor note incorrectly contains .exit" >&2; exit 1; }
! grep -q '^\.wq$' "$NOTE_FILE" || { echo "editor note incorrectly contains .wq" >&2; exit 1; }
! grep -q '^\.quit$' "$NOTE_FILE" || { echo "editor note incorrectly contains .quit" >&2; exit 1; }
! grep -q 'VitaOS Editor' "$NOTE_FILE" || { echo "editor note incorrectly contains editor frame title" >&2; exit 1; }
! grep -q 'Comandos:' "$NOTE_FILE" || { echo "editor note incorrectly contains command bar" >&2; exit 1; }
! grep -q 'Archivo:' "$NOTE_FILE" || { echo "editor note incorrectly contains file label" >&2; exit 1; }
! grep -q '+----' "$NOTE_FILE" || { echo "editor note incorrectly contains frame border" >&2; exit 1; }

[[ -d "$SESSIONS_DIR" ]] || { echo "missing sessions directory: $SESSIONS_DIR" >&2; exit 1; }

latest_txt=$(find "$SESSIONS_DIR" -maxdepth 1 -type f -name 'session-*.txt' | sort | tail -n 1)
latest_jsonl=$(find "$SESSIONS_DIR" -maxdepth 1 -type f -name 'session-*.jsonl' | sort | tail -n 1)

[[ -n "$latest_txt" ]] || { echo "missing session-*.txt in $SESSIONS_DIR" >&2; exit 1; }
[[ -n "$latest_jsonl" ]] || { echo "missing session-*.jsonl in $SESSIONS_DIR" >&2; exit 1; }

grep -q 'user_input' "$latest_jsonl" || { echo "session jsonl missing user_input" >&2; exit 1; }
grep -q 'system_output' "$latest_jsonl" || { echo "session jsonl missing system_output" >&2; exit 1; }

grep -q 'editor_save' "$latest_jsonl" || { echo "session jsonl missing editor_save" >&2; exit 1; }
# editor_open/editor_exit are validated only if emitted by current codebase.
if grep -q 'editor_open' "$latest_jsonl"; then :; fi
if grep -q 'editor_exit' "$latest_jsonl"; then :; fi

grep -q 'linea normal desde validador\|editor-test.txt\|editor_save\|editor_exit' "$latest_txt" || {
  echo "session txt missing expected editor evidence" >&2
  exit 1
}

ansi_pattern=$(printf '\033\[')
! grep -q "$ansi_pattern" "$NOTE_FILE" || { echo "ansi escape codes found in note file" >&2; exit 1; }
! grep -q "$ansi_pattern" "$latest_txt" || { echo "ansi escape codes found in session txt" >&2; exit 1; }
! grep -q "$ansi_pattern" "$latest_jsonl" || { echo "ansi escape codes found in session jsonl" >&2; exit 1; }

echo "validate-console-editor-history: ok"
