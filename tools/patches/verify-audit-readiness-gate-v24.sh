#!/usr/bin/env bash
set -euo pipefail

required=(
  "include/vita/audit.h"
  "kernel/audit_core.c"
  "kernel/command_core.c"
  "docs/audit/audit-readiness-gate.md"
)

for f in "${required[@]}"; do
  if [[ ! -f "$f" ]]; then
    echo "[v24 verify] missing file: $f" >&2
    exit 1
  fi
done

grep -q 'vita_audit_readiness_t' include/vita/audit.h
grep -q 'audit_is_persistent_ready' include/vita/audit.h
grep -q 'audit_show_readiness_report' include/vita/audit.h
grep -q 'VITA_AUDIT_READINESS_HOSTED_START' kernel/audit_core.c
grep -q 'VITA_AUDIT_READINESS_UEFI_START' kernel/audit_core.c
grep -q 'audit readiness' kernel/command_core.c
grep -q 'audit_show_readiness_report();' kernel/command_core.c

if grep -RInE '(^#!.*python|python3|[^A-Za-z0-9_]python[^A-Za-z0-9_])' \
  include/vita/audit.h kernel/audit_core.c kernel/command_core.c \
  docs/audit/audit-readiness-gate.md 2>/dev/null; then
  echo "[v24 verify] ERROR: unexpected Python reference" >&2
  exit 1
fi

echo "[v24 verify] OK: audit readiness gate patch is present"
