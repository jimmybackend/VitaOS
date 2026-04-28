#!/usr/bin/env bash
set -euo pipefail

# VitaOS patch v27: export audit hash-chain verification report.
# Bash/perl only. No Python, no malloc, no GUI/window manager.

ROOT=$(pwd)
AUDIT_H="$ROOT/include/vita/audit.h"
AUDIT_C="$ROOT/kernel/audit_core.c"
COMMAND_C="$ROOT/kernel/command_core.c"
DOC_DIR="$ROOT/docs/audit"
DOC="$DOC_DIR/audit-verification-export.md"

if [[ ! -f "$AUDIT_H" || ! -f "$AUDIT_C" || ! -f "$COMMAND_C" || ! -f "$ROOT/Makefile" ]]; then
  echo "[v27] ERROR: run this from the VitaOS repository root" >&2
  exit 1
fi

if ! grep -q 'vita_audit_hash_chain_result_t' "$AUDIT_H"; then
  echo "[v27] ERROR: v26 audit hash-chain verification must be applied before v27" >&2
  exit 1
fi

mkdir -p "$DOC_DIR"

# audit_core needs storage_write_text for report export.
if ! grep -q '#include <vita/storage.h>' "$AUDIT_C"; then
  perl -0pi -e 's/#include <vita\/audit.h>\n/#include <vita\/audit.h>\n#include <vita\/storage.h>\n/' "$AUDIT_C"
fi

# Public API for exporting/showing the verification report.
if ! grep -q 'audit_export_hash_chain_report' "$AUDIT_H"; then
  perl -0pi -e 's/(void audit_show_hash_chain_report\(void\);\n)/$1bool audit_export_hash_chain_report(void);\nvoid audit_show_export_hash_chain_report(void);\n/' "$AUDIT_H"
fi

# Implementation: append-only report builder that calls the v26 verifier.
if ! grep -q 'VITA_PATCH_V27_AUDIT_EXPORT_START' "$AUDIT_C"; then
  cat >> "$AUDIT_C" <<'CADD'

/* VITA_PATCH_V27_AUDIT_EXPORT_START */

#define AUDIT_VERIFY_TEXT_EXPORT_PATH  "/vita/export/audit/audit-verify.txt"
#define AUDIT_VERIFY_JSONL_EXPORT_PATH "/vita/export/audit/audit-verify.jsonl"

static void audit_export_reset_buffer(char *buf, unsigned long cap) {
    unsigned long i;

    if (!buf || cap == 0U) {
        return;
    }

    for (i = 0; i < cap; ++i) {
        buf[i] = '\0';
    }
}

static void audit_export_append(char *buf,
                                unsigned long cap,
                                unsigned long *len,
                                const char *text) {
    unsigned long i;

    if (!buf || cap == 0U || !len || !text) {
        return;
    }

    for (i = 0; text[i]; ++i) {
        if ((*len + 1U) >= cap) {
            buf[*len] = '\0';
            return;
        }
        buf[*len] = text[i];
        (*len)++;
    }

    buf[*len] = '\0';
}

static void audit_export_append_line(char *buf,
                                     unsigned long cap,
                                     unsigned long *len,
                                     const char *text) {
    audit_export_append(buf, cap, len, text ? text : "");
    audit_export_append(buf, cap, len, "\n");
}

static void audit_export_append_kv(char *buf,
                                   unsigned long cap,
                                   unsigned long *len,
                                   const char *key,
                                   const char *value) {
    audit_export_append(buf, cap, len, key ? key : "key");
    audit_export_append(buf, cap, len, ": ");
    audit_export_append_line(buf, cap, len, value ? value : "");
}

static void audit_export_append_json_string(char *buf,
                                            unsigned long cap,
                                            unsigned long *len,
                                            const char *text) {
    unsigned long i;

    if (!text) {
        return;
    }

    for (i = 0; text[i]; ++i) {
        char c = text[i];

        if (c == '"') {
            audit_export_append(buf, cap, len, "\\\"");
        } else if (c == '\\') {
            audit_export_append(buf, cap, len, "\\\\");
        } else if (c == '\n' || c == '\r') {
            audit_export_append(buf, cap, len, " ");
        } else if ((unsigned char)c < 32U) {
            audit_export_append(buf, cap, len, "?");
        } else {
            char one[2];
            one[0] = c;
            one[1] = '\0';
            audit_export_append(buf, cap, len, one);
        }
    }
}

static void audit_export_append_u64_kv(char *buf,
                                       unsigned long cap,
                                       unsigned long *len,
                                       const char *key,
                                       uint64_t value) {
    char num[32];

    audit_u64_to_dec(value, num);
    audit_export_append_kv(buf, cap, len, key, num);
}

bool audit_export_hash_chain_report(void) {
    static char text[2048];
    static char jsonl[1024];
    unsigned long text_len = 0U;
    unsigned long json_len = 0U;
    vita_audit_hash_chain_result_t result;
    char num[32];
    bool ok_text;
    bool ok_jsonl;

    audit_export_reset_buffer(text, sizeof(text));
    audit_export_reset_buffer(jsonl, sizeof(jsonl));

    (void)audit_verify_hash_chain(&result);

    audit_export_append_line(text, sizeof(text), &text_len, "VitaOS audit hash-chain verification export");
    audit_export_append_line(text, sizeof(text), &text_len, "Reporte de verificacion de cadena hash de auditoria");
    audit_export_append_line(text, sizeof(text), &text_len, "");
    audit_export_append_kv(text, sizeof(text), &text_len, "text_path", AUDIT_VERIFY_TEXT_EXPORT_PATH);
    audit_export_append_kv(text, sizeof(text), &text_len, "jsonl_path", AUDIT_VERIFY_JSONL_EXPORT_PATH);
    audit_export_append_kv(text, sizeof(text), &text_len, "available", result.available ? "yes" : "no");
    audit_export_append_kv(text, sizeof(text), &text_len, "ok", result.ok ? "yes" : "no");
    audit_export_append_u64_kv(text, sizeof(text), &text_len, "checked_events", result.checked_events);
    audit_export_append_u64_kv(text, sizeof(text), &text_len, "first_seq", result.first_seq);
    audit_export_append_u64_kv(text, sizeof(text), &text_len, "last_seq", result.last_seq);
    audit_export_append_kv(text, sizeof(text), &text_len, "first_bad_event", result.first_bad_event[0] ? result.first_bad_event : "none");
    audit_export_append_kv(text, sizeof(text), &text_len, "message", result.message[0] ? result.message : "none");
    audit_export_append_line(text, sizeof(text), &text_len, "");
    audit_export_append_line(text, sizeof(text), &text_len, "Scope: report/export only; no audit rows were repaired, rewritten, deleted or mutated.");

    audit_export_append(jsonl, sizeof(jsonl), &json_len, "{\"type\":\"audit_hash_chain_verification\",");
    audit_export_append(jsonl, sizeof(jsonl), &json_len, "\"available\":");
    audit_export_append(jsonl, sizeof(jsonl), &json_len, result.available ? "true" : "false");
    audit_export_append(jsonl, sizeof(jsonl), &json_len, ",\"ok\":");
    audit_export_append(jsonl, sizeof(jsonl), &json_len, result.ok ? "true" : "false");
    audit_export_append(jsonl, sizeof(jsonl), &json_len, ",\"checked_events\":");
    audit_u64_to_dec(result.checked_events, num);
    audit_export_append(jsonl, sizeof(jsonl), &json_len, num);
    audit_export_append(jsonl, sizeof(jsonl), &json_len, ",\"first_seq\":");
    audit_u64_to_dec(result.first_seq, num);
    audit_export_append(jsonl, sizeof(jsonl), &json_len, num);
    audit_export_append(jsonl, sizeof(jsonl), &json_len, ",\"last_seq\":");
    audit_u64_to_dec(result.last_seq, num);
    audit_export_append(jsonl, sizeof(jsonl), &json_len, num);
    audit_export_append(jsonl, sizeof(jsonl), &json_len, ",\"first_bad_event\":\"");
    audit_export_append_json_string(jsonl, sizeof(jsonl), &json_len, result.first_bad_event[0] ? result.first_bad_event : "none");
    audit_export_append(jsonl, sizeof(jsonl), &json_len, "\",\"message\":\"");
    audit_export_append_json_string(jsonl, sizeof(jsonl), &json_len, result.message[0] ? result.message : "none");
    audit_export_append(jsonl, sizeof(jsonl), &json_len, "\"}\n");

    ok_text = storage_write_text(AUDIT_VERIFY_TEXT_EXPORT_PATH, text);
    ok_jsonl = storage_write_text(AUDIT_VERIFY_JSONL_EXPORT_PATH, jsonl);

    return ok_text && ok_jsonl;
}

void audit_show_export_hash_chain_report(void) {
    console_write_line("Audit verification export / Exportacion de verificacion de auditoria:");

    if (audit_export_hash_chain_report()) {
        console_write_line("export: ok");
        console_write_line(AUDIT_VERIFY_TEXT_EXPORT_PATH);
        console_write_line(AUDIT_VERIFY_JSONL_EXPORT_PATH);
        return;
    }

    console_write_line("export: failed");
    console_write_line("Check storage readiness and /vita/export/audit/ tree.");
    console_write_line("Revisa storage readiness y el arbol /vita/export/audit/.");
}

/* VITA_PATCH_V27_AUDIT_EXPORT_END */
CADD
fi

# Command dispatch for export aliases.
if ! grep -q 'audit_show_export_hash_chain_report' "$COMMAND_C"; then
  perl -0pi -e 's/(    if \(str_eq\(cmd, "peers"\)\) \{)/    if (str_eq(cmd, "audit export") ||\n        str_eq(cmd, "audit verify export") ||\n        str_eq(cmd, "audit export verify") ||\n        str_eq(cmd, "export audit") ||\n        str_eq(cmd, "export audit verify")) {\n        audit_emit_boot_event("COMMAND_AUDIT_EXPORT", "audit export");\n        audit_show_export_hash_chain_report();\n        return VITA_COMMAND_CONTINUE;\n    }\n\n$1/' "$COMMAND_C"
fi

cat > "$DOC" <<'DOC'
# Audit verification export

Patch: `audit: export hash-chain verification report`

## Purpose

VitaOS already has a console-visible audit hash-chain verifier in v26.

This patch adds a persistent export for that verification result so the operator can inspect it later from the writable USB tree.

## Commands

```text
audit export
audit verify export
audit export verify
export audit
export audit verify
```

## Output files

```text
/vita/export/audit/audit-verify.txt
/vita/export/audit/audit-verify.jsonl
```

## Hosted behavior

Hosted mode can verify the current boot's SQLite `audit_event` chain and export the result.

## UEFI behavior

UEFI exports the honest diagnostic status: hash-chain verification remains unavailable until the freestanding UEFI path has real persistent SQLite audit.

## Scope boundaries

This patch does not mutate audit rows. It reports and exports only.

Not included:

- UEFI SQLite persistence;
- multi-boot historical database verification;
- row repair;
- Python;
- malloc;
- GUI/window manager;
- networking or Wi-Fi expansion.
DOC

# Basic sanity checks.
for sym in \
  'audit_export_hash_chain_report' \
  'audit_show_export_hash_chain_report'; do
  if ! grep -q "$sym" "$AUDIT_H" "$AUDIT_C"; then
    echo "[v27] ERROR: missing expected symbol: $sym" >&2
    exit 1
  fi
done

if ! grep -q 'export audit verify' "$COMMAND_C"; then
  echo "[v27] ERROR: command_core.c missing export audit verify alias" >&2
  exit 1
fi

if grep -RInE '(^#!.*python|python3|[^A-Za-z0-9_]python[^A-Za-z0-9_])' "$AUDIT_H" "$AUDIT_C" "$COMMAND_C" "$DOC" 2>/dev/null; then
  echo "[v27] ERROR: unexpected Python reference in v27 files" >&2
  exit 1
fi

echo "[v27] audit verification export patch applied"
