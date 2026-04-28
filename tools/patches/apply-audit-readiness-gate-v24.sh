#!/usr/bin/env bash
set -euo pipefail

# VitaOS patch v24: audit readiness gate report.
# Bash/perl only. No Python, no malloc, no GUI/window manager.

ROOT=$(pwd)
AUDIT_H="$ROOT/include/vita/audit.h"
AUDIT_C="$ROOT/kernel/audit_core.c"
COMMAND_C="$ROOT/kernel/command_core.c"
DOC_DIR="$ROOT/docs/audit"
DOC="$DOC_DIR/audit-readiness-gate.md"

if [[ ! -f "$ROOT/Makefile" || ! -f "$AUDIT_H" || ! -f "$AUDIT_C" || ! -f "$COMMAND_C" ]]; then
  echo "[v24] ERROR: run this from the VitaOS repository root" >&2
  exit 1
fi

mkdir -p "$DOC_DIR"

# Public readiness status contract.
if ! grep -q 'vita_audit_readiness_t' "$AUDIT_H"; then
  perl -0pi -e 's/(#include <vita\/proposal\.h>\n)/$1\n\
typedef struct {\n\
    bool persistent_ready;\n\
    bool restricted_diagnostic;\n\
    bool sqlite_backend;\n\
    const char *backend_name;\n\
    const char *mode;\n\
    const char *audit_state;\n\
    const char *required_action;\n\
} vita_audit_readiness_t;\n/' "$AUDIT_H"
fi

if ! grep -q 'audit_show_readiness_report' "$AUDIT_H"; then
  perl -0pi -e 's/(bool audit_init_persistent_backend\(const vita_handoff_t \*handoff\);\n)/$1bool audit_is_persistent_ready(void);\nvoid audit_get_readiness(vita_audit_readiness_t *out_status);\nvoid audit_show_readiness_report(void);\n/' "$AUDIT_H"
fi

# The report prints through the existing text console path.
if ! grep -q '#include <vita/console.h>' "$AUDIT_C"; then
  perl -0pi -e 's/#include <vita\/audit\.h>\n/#include <vita\/audit.h>\n#include <vita\/console.h>\n/' "$AUDIT_C"
fi

# Hosted implementation: SQLite backend can be READY.
if ! grep -q 'VITA_AUDIT_READINESS_HOSTED_START' "$AUDIT_C"; then
  perl -0pi -e 's/\n#else\n/\n\n\/\* VITA_AUDIT_READINESS_HOSTED_START \*\/\nbool audit_is_persistent_ready(void) {\n    return g_audit.persistent_ready && g_audit.db != 0;\n}\n\nvoid audit_get_readiness(vita_audit_readiness_t *out_status) {\n    if (!out_status) {\n        return;\n    }\n\n    out_status->persistent_ready = audit_is_persistent_ready();\n    out_status->restricted_diagnostic = !out_status->persistent_ready;\n    out_status->sqlite_backend = out_status->persistent_ready;\n    out_status->backend_name = out_status->persistent_ready ? \"hosted_sqlite\" : \"none\";\n    out_status->mode = out_status->persistent_ready ? \"OPERATIONAL\" : \"RESTRICTED_DIAGNOSTIC\";\n    out_status->audit_state = out_status->persistent_ready ? \"READY\" : \"FAILED\";\n    out_status->required_action = out_status->persistent_ready\n        ? \"none - persistent SQLite audit is active\"\n        : \"provide a writable persistent audit backend before full operation\";\n}\n\nvoid audit_show_readiness_report(void) {\n    vita_audit_readiness_t st;\n\n    audit_get_readiness(&st);\n\n    console_write_line(\"Audit readiness gate / Puerta de auditoria:\");\n    console_write_line(\"persistent_ready:\");\n    console_write_line(st.persistent_ready ? \"yes\" : \"no\");\n    console_write_line(\"restricted_diagnostic:\");\n    console_write_line(st.restricted_diagnostic ? \"yes\" : \"no\");\n    console_write_line(\"sqlite_backend:\");\n    console_write_line(st.sqlite_backend ? \"yes\" : \"no\");\n    console_write_line(\"backend:\");\n    console_write_line(st.backend_name ? st.backend_name : \"unknown\");\n    console_write_line(\"mode:\");\n    console_write_line(st.mode ? st.mode : \"unknown\");\n    console_write_line(\"audit_state:\");\n    console_write_line(st.audit_state ? st.audit_state : \"unknown\");\n    console_write_line(\"required_action:\");\n    console_write_line(st.required_action ? st.required_action : \"unknown\");\n\n    if (st.persistent_ready) {\n        console_write_line(\"Full operational mode may continue because persistent audit is active.\");\n        console_write_line(\"El modo operativo completo puede continuar porque la auditoria persistente esta activa.\");\n    } else {\n        console_write_line(\"Full operational mode must stay blocked until persistent audit is available.\");\n        console_write_line(\"El modo operativo completo debe seguir bloqueado hasta tener auditoria persistente.\");\n    }\n}\n\/\* VITA_AUDIT_READINESS_HOSTED_END \*\/\n\n#else\n/' "$AUDIT_C"
fi

# UEFI/freestanding implementation: explicit restricted diagnostic until SQLite/persistent backend exists.
if ! grep -q 'VITA_AUDIT_READINESS_UEFI_START' "$AUDIT_C"; then
  perl -0pi -e 's/\n#endif\n\z/\n\n\/\* VITA_AUDIT_READINESS_UEFI_START \*\/\nbool audit_is_persistent_ready(void) {\n    return false;\n}\n\nvoid audit_get_readiness(vita_audit_readiness_t *out_status) {\n    if (!out_status) {\n        return;\n    }\n\n    out_status->persistent_ready = false;\n    out_status->restricted_diagnostic = true;\n    out_status->sqlite_backend = false;\n    out_status->backend_name = \"uefi_stub\";\n    out_status->mode = \"RESTRICTED_DIAGNOSTIC\";\n    out_status->audit_state = \"FAILED\";\n    out_status->required_action = \"UEFI persistent SQLite audit backend is not implemented yet\";\n}\n\nvoid audit_show_readiness_report(void) {\n    vita_audit_readiness_t st;\n\n    audit_get_readiness(&st);\n\n    console_write_line(\"Audit readiness gate / Puerta de auditoria:\");\n    console_write_line(\"persistent_ready:\");\n    console_write_line(\"no\");\n    console_write_line(\"restricted_diagnostic:\");\n    console_write_line(\"yes\");\n    console_write_line(\"sqlite_backend:\");\n    console_write_line(\"no\");\n    console_write_line(\"backend:\");\n    console_write_line(st.backend_name);\n    console_write_line(\"mode:\");\n    console_write_line(st.mode);\n    console_write_line(\"audit_state:\");\n    console_write_line(st.audit_state);\n    console_write_line(\"required_action:\");\n    console_write_line(st.required_action);\n    console_write_line(\"UEFI can run local diagnostic commands, storage, journal bridge and export helpers.\");\n    console_write_line(\"UEFI puede ejecutar diagnostico local, storage, journal temporal y export helpers.\");\n    console_write_line(\"It must not claim full operational mode until persistent SQLite audit exists.\");\n    console_write_line(\"No debe anunciar modo operativo completo hasta tener SQLite persistente.\");\n}\n\/\* VITA_AUDIT_READINESS_UEFI_END \*\/\n#endif\n/' "$AUDIT_C"
fi

# Replace old manual audit output with the central readiness report.
if ! grep -q 'audit_show_readiness_report();' "$COMMAND_C"; then
  perl -0pi -e 's/static void show_audit\(const vita_command_context_t \*ctx\) \{\n.*?\n\}\n\nstatic void show_peers/static void show_audit(const vita_command_context_t *ctx) {\n    (void)ctx;\n    audit_show_readiness_report();\n}\n\nstatic void show_peers/s' "$COMMAND_C"
fi

# Add explicit audit aliases.
if ! grep -q 'audit readiness' "$COMMAND_C"; then
  perl -0pi -e 's/if \(str_eq\(cmd, "audit"\)\) \{/if (str_eq(cmd, "audit") ||\n        str_eq(cmd, "audit status") ||\n        str_eq(cmd, "audit report") ||\n        str_eq(cmd, "audit gate") ||\n        str_eq(cmd, "audit readiness")) {/g' "$COMMAND_C"
fi

cat > "$DOC" <<'DOC'
# Audit readiness gate

Patch: `audit: add UEFI audit readiness gate report`

## Purpose

VitaOS is audit-first. Full operational mode must not be claimed unless a persistent audit backend is active.

This patch adds a visible readiness report so the operator can distinguish:

- hosted SQLite audit ready;
- UEFI restricted diagnostic mode while persistent SQLite audit is still pending.

## Commands

```text
audit
audit status
audit report
audit gate
audit readiness
```

## Expected behavior

Hosted mode may report:

```text
persistent_ready: yes
backend: hosted_sqlite
mode: OPERATIONAL
audit_state: READY
```

UEFI mode currently reports:

```text
persistent_ready: no
restricted_diagnostic: yes
backend: uefi_stub
mode: RESTRICTED_DIAGNOSTIC
audit_state: FAILED
```

This is intentional until a real persistent SQLite backend is implemented for UEFI.

## Scope boundaries

Included:

- central audit readiness status;
- visible bilingual console report;
- hosted/UEFI distinction;
- explicit gate wording.

Not included:

- UEFI SQLite persistence;
- hash-chain verification command;
- Python;
- malloc;
- GUI/window manager;
- network or Wi-Fi expansion.
DOC

cat > "$ROOT/README-vitaos-patch24-audit-readiness.md" <<'README'
# VitaOS patch v24 — audit readiness gate report

Commit title:

```text
audit: add UEFI audit readiness gate report
```

## What this changes

Adds a central audit readiness report and uses it from the `audit` command.

New aliases:

```text
audit status
audit report
audit gate
audit readiness
```

Hosted reports persistent SQLite audit readiness when the SQLite backend is active.

UEFI reports restricted diagnostic mode explicitly until a real persistent SQLite audit backend exists there.

## Apply

```bash
bash tools/patches/apply-audit-readiness-gate-v24.sh
```

## Validate

```bash
bash tools/patches/verify-audit-readiness-gate-v24.sh
make clean
make hosted
make smoke-audit
make
make smoke
make iso
```

## Scope

No Python, no malloc, no GUI/window manager, no network/Wi-Fi expansion.
README

cat > "$ROOT/tools/patches/verify-audit-readiness-gate-v24.sh" <<'VERIFY'
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
VERIFY
chmod +x "$ROOT/tools/patches/verify-audit-readiness-gate-v24.sh"

# Final sanity checks.
bash "$ROOT/tools/patches/verify-audit-readiness-gate-v24.sh"

echo "[v24] audit readiness gate patch applied"
