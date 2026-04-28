#!/usr/bin/env bash
set -euo pipefail

ROOT=$(pwd)
DOC="$ROOT/docs/validation/f1a-f1b-validation-checklist.md"

if [[ ! -f "$ROOT/Makefile" || ! -d "$ROOT/kernel" || ! -d "$ROOT/include" ]]; then
  echo "[verify-v32] ERROR: run from VitaOS repository root" >&2
  exit 1
fi

if [[ ! -f "$DOC" ]]; then
  echo "[verify-v32] ERROR: missing docs/validation/f1a-f1b-validation-checklist.md" >&2
  exit 1
fi

required=(
  "make clean"
  "make hosted"
  "make smoke-audit"
  "make iso"
  "storage repair"
  "audit verify"
  "audit sqlite"
  "export index"
  "selftest"
  "UEFI persistent audit backend"
)

for sym in "${required[@]}"; do
  if ! grep -q "$sym" "$DOC"; then
    echo "[verify-v32] ERROR: checklist missing: $sym" >&2
    exit 1
  fi
done

if grep -RInE '(^#!.*python|python3|[^A-Za-z0-9_]python[^A-Za-z0-9_])' "$DOC" 2>/dev/null; then
  echo "[verify-v32] ERROR: unexpected Python reference in checklist" >&2
  exit 1
fi

echo "[verify-v32] OK: F1A/F1B validation checklist present"
