#!/usr/bin/env bash
set -euo pipefail

ROOT=$(pwd)
JOURNAL_C="$ROOT/kernel/session_journal.c"
JOURNAL_H="$ROOT/include/vita/session_journal.h"
DOC="$ROOT/docs/audit/journal-recovery-summary.md"

if [[ ! -f "$ROOT/Makefile" || ! -f "$JOURNAL_C" || ! -f "$JOURNAL_H" ]]; then
  echo "[verify-v23] ERROR: run this from the VitaOS repository root" >&2
  exit 1
fi

missing=0
for sym in \
  'SESSION_JOURNAL_RECOVERY_MAX' \
  'recovered_available' \
  'session_journal_capture_previous' \
  'session_journal_show_summary' \
  'session_journal_recover_last' \
  'journal summary' \
  'journal last-session' \
  'journal recover' \
  'previous_session_captured'; do
  if ! grep -q "$sym" "$JOURNAL_C" "$JOURNAL_H" 2>/dev/null; then
    echo "[verify-v23] missing: $sym" >&2
    missing=1
  fi
done

if [[ ! -f "$DOC" ]]; then
  echo "[verify-v23] missing doc: docs/audit/journal-recovery-summary.md" >&2
  missing=1
fi

if grep -RInE '(^#!.*python|python3|[^A-Za-z0-9_]python[^A-Za-z0-9_])' "$JOURNAL_C" "$JOURNAL_H" 2>/dev/null; then
  echo "[verify-v23] ERROR: unexpected Python reference in v23 runtime files" >&2
  missing=1
fi

if (( missing != 0 )); then
  exit 1
fi

echo "[verify-v23] OK: journal recovery and last-session summary symbols present"
