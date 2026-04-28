#!/usr/bin/env bash
set -euo pipefail

ROOT=$(pwd)
AUDIT_H="$ROOT/include/vita/audit.h"
AUDIT_C="$ROOT/kernel/audit_core.c"
COMMAND_C="$ROOT/kernel/command_core.c"
DOC="$ROOT/docs/audit/sqlite-summary.md"

if [[ ! -f "$AUDIT_H" || ! -f "$AUDIT_C" || ! -f "$COMMAND_C" ]]; then
  echo "[verify-v29] ERROR: run from VitaOS repository root" >&2
  exit 1
fi

required=(
  "vita_audit_sqlite_summary_t"
  "audit_get_sqlite_summary"
  "audit_show_sqlite_summary"
  "audit_sqlite_summary_count_sql"
  "audit sqlite summary"
)

for sym in "${required[@]}"; do
  if ! grep -Rqs "$sym" "$AUDIT_H" "$AUDIT_C" "$COMMAND_C" "$DOC" 2>/dev/null; then
    echo "[verify-v29] ERROR: missing $sym" >&2
    exit 1
  fi
done

if grep -RInE '(^#!.*python|python3|[^A-Za-z0-9_]python[^A-Za-z0-9_])' \
  "$AUDIT_H" "$AUDIT_C" "$COMMAND_C" "$DOC" 2>/dev/null; then
  echo "[verify-v29] ERROR: unexpected Python reference" >&2
  exit 1
fi

echo "[verify-v29] OK: hosted SQLite audit summary patch present"
