#!/usr/bin/env bash
set -euo pipefail

ROOT=$(pwd)
AUDIT_H="$ROOT/include/vita/audit.h"
AUDIT_C="$ROOT/kernel/audit_core.c"
COMMAND_C="$ROOT/kernel/command_core.c"
DOC="$ROOT/docs/audit/audit-hash-chain-verification.md"

if [[ ! -f "$AUDIT_H" || ! -f "$AUDIT_C" || ! -f "$COMMAND_C" ]]; then
  echo "[verify-v26] ERROR: run from VitaOS repository root" >&2
  exit 1
fi

needles=(
  "vita_audit_hash_chain_result_t:$AUDIT_H"
  "audit_verify_hash_chain:$AUDIT_H"
  "audit_show_hash_chain_report:$AUDIT_H"
  "VITA_PATCH_V26_AUDIT_HASH_CHAIN_START:$AUDIT_C"
  "hash chain verified for current boot:$AUDIT_C"
  "audit hash-chain:$COMMAND_C"
)

for item in "${needles[@]}"; do
  needle="${item%%:*}"
  file="${item#*:}"
  if ! grep -q "$needle" "$file"; then
    echo "[verify-v26] ERROR: missing '$needle' in $file" >&2
    exit 1
  fi
done

if [[ ! -f "$DOC" ]]; then
  echo "[verify-v26] ERROR: missing docs/audit/audit-hash-chain-verification.md" >&2
  exit 1
fi

if grep -RInE '(^#!.*python|python3|[^A-Za-z0-9_]python[^A-Za-z0-9_])' \
  "$AUDIT_H" "$AUDIT_C" "$COMMAND_C" "$DOC" 2>/dev/null; then
  echo "[verify-v26] ERROR: unexpected Python reference" >&2
  exit 1
fi

echo "[verify-v26] OK"
