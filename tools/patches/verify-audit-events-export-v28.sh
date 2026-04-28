#!/usr/bin/env bash
set -euo pipefail

ROOT=$(pwd)
AUDIT_H="$ROOT/include/vita/audit.h"
AUDIT_C="$ROOT/kernel/audit_core.c"
COMMAND_C="$ROOT/kernel/command_core.c"
DOC="$ROOT/docs/audit/audit-current-session-events-export.md"

if [[ ! -f "$AUDIT_H" || ! -f "$AUDIT_C" || ! -f "$COMMAND_C" ]]; then
  echo "[verify-v28] ERROR: run from VitaOS repository root" >&2
  exit 1
fi

required=(
  "audit_export_current_session_events"
  "audit_show_current_session_events_export"
  "AUDIT_EVENTS_TEXT_EXPORT_PATH"
  "AUDIT_EVENTS_JSONL_EXPORT_PATH"
  "export audit events"
  "current-session-events.jsonl"
)

for sym in "${required[@]}"; do
  if ! grep -Rqs "$sym" "$AUDIT_H" "$AUDIT_C" "$COMMAND_C" "$DOC" 2>/dev/null; then
    echo "[verify-v28] ERROR: missing $sym" >&2
    exit 1
  fi
done

if grep -RInE '(^#!.*python|python3|[^A-Za-z0-9_]python[^A-Za-z0-9_])' \
  "$AUDIT_H" "$AUDIT_C" "$COMMAND_C" "$DOC" 2>/dev/null; then
  echo "[verify-v28] ERROR: unexpected Python reference" >&2
  exit 1
fi

echo "[verify-v28] OK: current session audit events export patch present"
