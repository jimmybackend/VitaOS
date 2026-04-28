#!/usr/bin/env bash
set -euo pipefail

ROOT=$(pwd)
REQUIRED=(
  "Makefile"
  "include/vita/diagnostic_bundle.h"
  "kernel/diagnostic_bundle.c"
  "kernel/command_core.c"
  "docs/export/diagnostic-bundle.md"
)

if [[ ! -f "$ROOT/Makefile" || ! -d "$ROOT/kernel" || ! -d "$ROOT/include/vita" ]]; then
  echo "[verify-v25] ERROR: run this from the VitaOS repository root" >&2
  exit 1
fi

for f in "${REQUIRED[@]}"; do
  if [[ ! -f "$ROOT/$f" ]]; then
    echo "[verify-v25] ERROR: missing $f" >&2
    exit 1
  fi
done

checks=(
  "kernel/diagnostic_bundle.c"
  "diagnostic_bundle_handle_command"
  "diagnostic_bundle_export"
  "VITA_DIAGNOSTIC_BUNDLE_TEXT_PATH"
  "diagnostic-bundle.jsonl"
  "DIAGNOSTIC_BUNDLE_EXPORTED"
)

for sym in "${checks[@]}"; do
  if ! grep -RIn "$sym" Makefile include/vita/diagnostic_bundle.h kernel/diagnostic_bundle.c kernel/command_core.c docs/export/diagnostic-bundle.md >/dev/null 2>&1; then
    echo "[verify-v25] ERROR: expected symbol missing: $sym" >&2
    exit 1
  fi
done

if grep -RInE '(^#!.*python|python3|[^A-Za-z0-9_]python[^A-Za-z0-9_])' Makefile include/vita/diagnostic_bundle.h kernel/diagnostic_bundle.c kernel/command_core.c tools/build tools/image tools/test 2>/dev/null; then
  echo "[verify-v25] ERROR: unexpected Python reference in runtime/build paths" >&2
  exit 1
fi

echo "[verify-v25] OK: diagnostic bundle patch is present"
