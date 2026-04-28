#!/usr/bin/env bash
set -euo pipefail

ROOT=$(pwd)
MAKEFILE="$ROOT/Makefile"
HEADER="$ROOT/include/vita/export_manifest.h"
SOURCE="$ROOT/kernel/export_manifest.c"
COMMAND_C="$ROOT/kernel/command_core.c"
DOC="$ROOT/docs/export/export-index-manifest.md"

if [[ ! -f "$MAKEFILE" || ! -f "$HEADER" || ! -f "$SOURCE" || ! -f "$COMMAND_C" ]]; then
  echo "[verify-v30] ERROR: run from VitaOS repository root after applying v30" >&2
  exit 1
fi

required=(
  "kernel/export_manifest.c"
  "export_manifest_write"
  "export_manifest_show"
  "export_manifest_handle_command"
  "EXPORT_INDEX_TEXT_PATH"
  "EXPORT_INDEX_JSONL_PATH"
  "export index"
  "storage export-index"
)

for sym in "${required[@]}"; do
  if ! grep -Rqs "$sym" "$MAKEFILE" "$HEADER" "$SOURCE" "$COMMAND_C" "$DOC" 2>/dev/null; then
    echo "[verify-v30] ERROR: missing $sym" >&2
    exit 1
  fi
done

if grep -RInE '(^#!.*python|python3|[^A-Za-z0-9_]python[^A-Za-z0-9_])' \
  "$HEADER" "$SOURCE" "$COMMAND_C" "$DOC" 2>/dev/null; then
  echo "[verify-v30] ERROR: unexpected Python reference" >&2
  exit 1
fi

echo "[verify-v30] OK: export index manifest patch present"
