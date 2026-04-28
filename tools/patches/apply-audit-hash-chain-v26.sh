#!/usr/bin/env bash
set -euo pipefail

# VitaOS patch v26: audit hash-chain verification command.
# Bash/perl only. No Python, no GUI/window manager, no new runtime dependency.

ROOT=$(pwd)
AUDIT_H="$ROOT/include/vita/audit.h"
AUDIT_C="$ROOT/kernel/audit_core.c"
COMMAND_C="$ROOT/kernel/command_core.c"
DOC_DIR="$ROOT/docs/audit"
DOC="$DOC_DIR/audit-hash-chain-verification.md"

if [[ ! -f "$AUDIT_H" || ! -f "$AUDIT_C" || ! -f "$COMMAND_C" || ! -f "$ROOT/Makefile" ]]; then
  echo "[v26] ERROR: run this from the VitaOS repository root" >&2
  exit 1
fi

mkdir -p "$DOC_DIR"

# Public result struct and API.
if ! grep -q 'vita_audit_hash_chain_result_t' "$AUDIT_H"; then
  perl -0pi -e 's/(bool audit_export_recent_event_block\(char \*out, unsigned long out_cap\);\n)/typedef struct {\n    bool available;\n    bool ok;\n    uint64_t checked_events;\n    uint64_t first_seq;\n    uint64_t last_seq;\n    char first_bad_event[96];\n    char message[160];\n} vita_audit_hash_chain_result_t;\n\nbool audit_verify_hash_chain(vita_audit_hash_chain_result_t *out_result);\nvoid audit_show_hash_chain_report(void);\n\n$1/' "$AUDIT_H"
fi

# audit_core needs console output for the report.
if ! grep -q '#include <vita/console.h>' "$AUDIT_C"; then
  perl -0pi -e 's/#include <vita\/audit.h>\n/#include <vita\/audit.h>\n#include <vita\/console.h>\n/' "$AUDIT_C"
fi

# Append implementation once. It is deliberately placed at file end so it can
# use hosted static helpers/state inside the same translation unit while keeping
# UEFI as a clean stub.
if ! grep -q 'VITA_PATCH_V26_AUDIT_HASH_CHAIN_START' "$AUDIT_C"; then
  cat >> "$AUDIT_C" <<'CADD'

/* VITA_PATCH_V26_AUDIT_HASH_CHAIN_START */

static void audit_u64_to_dec(uint64_t value, char out[32]) {
    char tmp[32];
    int i = 0;
    int j = 0;

    if (!out) {
        return;
    }

    if (value == 0U) {
        out[0] = '0';
        out[1] = '\0';
        return;
    }

    while (value > 0U && i < (int)(sizeof(tmp) - 1U)) {
        tmp[i++] = (char)('0' + (value % 10U));
        value /= 10U;
    }

    while (i > 0) {
        out[j++] = tmp[--i];
    }

    out[j] = '\0';
}

static void audit_result_clear(vita_audit_hash_chain_result_t *out_result) {
    unsigned long i;

    if (!out_result) {
        return;
    }

    out_result->available = false;
    out_result->ok = false;
    out_result->checked_events = 0U;
    out_result->first_seq = 0U;
    out_result->last_seq = 0U;

    for (i = 0; i < sizeof(out_result->first_bad_event); ++i) {
        out_result->first_bad_event[i] = '\0';
    }
    for (i = 0; i < sizeof(out_result->message); ++i) {
        out_result->message[i] = '\0';
    }
}

static void audit_result_copy(char *dst, unsigned long cap, const char *src) {
    unsigned long i = 0;

    if (!dst || cap == 0U) {
        return;
    }

    if (!src) {
        dst[0] = '\0';
        return;
    }

    while (src[i] && (i + 1U) < cap) {
        dst[i] = src[i];
        i++;
    }

    dst[i] = '\0';
}

#ifdef VITA_HOSTED
bool audit_verify_hash_chain(vita_audit_hash_chain_result_t *out_result) {
    const char *sql =
        "SELECT event_seq,event_type,severity,actor_type,summary,details_json,monotonic_ns,prev_hash,event_hash "
        "FROM audit_event "
        "WHERE boot_id=?1 "
        "ORDER BY event_seq ASC;";
    sqlite3_stmt *st = NULL;
    char expected_prev[AUDIT_HASH_HEX_LEN + 1];
    char payload[1024];
    char recomputed[AUDIT_HASH_HEX_LEN + 1];
    bool first = true;
    int rc;

    if (!out_result) {
        return false;
    }

    audit_result_clear(out_result);

    if (!g_audit.persistent_ready || !g_audit.db || !g_audit.boot_id[0]) {
        audit_result_copy(out_result->message, sizeof(out_result->message), "persistent audit backend unavailable");
        return false;
    }

    if (sqlite3_prepare_v2(g_audit.db, sql, -1, &st, NULL) != SQLITE_OK) {
        out_result->available = true;
        audit_result_copy(out_result->message, sizeof(out_result->message), "sqlite prepare failed");
        return false;
    }

    expected_prev[0] = '\0';
    sqlite3_bind_text(st, 1, g_audit.boot_id, -1, SQLITE_STATIC);

    out_result->available = true;
    out_result->ok = true;

    while ((rc = sqlite3_step(st)) == SQLITE_ROW) {
        uint64_t seq = (uint64_t)sqlite3_column_int64(st, 0);
        const char *etype = (const char *)sqlite3_column_text(st, 1);
        const char *severity = (const char *)sqlite3_column_text(st, 2);
        const char *actor = (const char *)sqlite3_column_text(st, 3);
        const char *summary = (const char *)sqlite3_column_text(st, 4);
        const char *details = (const char *)sqlite3_column_text(st, 5);
        uint64_t mono_ns = (uint64_t)sqlite3_column_int64(st, 6);
        const char *prev_hash = (const char *)sqlite3_column_text(st, 7);
        const char *event_hash = (const char *)sqlite3_column_text(st, 8);

        if (first) {
            out_result->first_seq = seq;
            first = false;
        }

        out_result->last_seq = seq;
        out_result->checked_events++;

        if (strcmp(prev_hash ? prev_hash : "", expected_prev) != 0) {
            out_result->ok = false;
            audit_result_copy(out_result->message, sizeof(out_result->message), "prev_hash mismatch");
            snprintf(out_result->first_bad_event,
                     sizeof(out_result->first_bad_event),
                     "seq=%llu",
                     (unsigned long long)seq);
            break;
        }

        snprintf(payload,
                 sizeof(payload),
                 "%s|%llu|%s|%s|%s|%s|%s|%llu|%s",
                 g_audit.boot_id,
                 (unsigned long long)seq,
                 etype ? etype : "UNSPECIFIED",
                 severity ? severity : "INFO",
                 actor ? actor : "SYSTEM",
                 summary ? summary : "",
                 details ? details : "{}",
                 (unsigned long long)mono_ns,
                 prev_hash ? prev_hash : "");

        hash_hex(payload, recomputed);

        if (strcmp(event_hash ? event_hash : "", recomputed) != 0) {
            out_result->ok = false;
            audit_result_copy(out_result->message, sizeof(out_result->message), "event_hash mismatch");
            snprintf(out_result->first_bad_event,
                     sizeof(out_result->first_bad_event),
                     "seq=%llu",
                     (unsigned long long)seq);
            break;
        }

        audit_result_copy(expected_prev, sizeof(expected_prev), recomputed);
    }

    sqlite3_finalize(st);

    if (rc != SQLITE_DONE && out_result->ok) {
        out_result->ok = false;
        audit_result_copy(out_result->message, sizeof(out_result->message), "sqlite step failed");
        return false;
    }

    if (out_result->checked_events == 0U) {
        out_result->ok = false;
        audit_result_copy(out_result->message, sizeof(out_result->message), "no audit_event rows for current boot");
        return false;
    }

    if (out_result->ok) {
        audit_result_copy(out_result->message, sizeof(out_result->message), "hash chain verified for current boot");
    }

    return out_result->ok;
}
#else
bool audit_verify_hash_chain(vita_audit_hash_chain_result_t *out_result) {
    if (!out_result) {
        return false;
    }

    audit_result_clear(out_result);
    audit_result_copy(out_result->message,
                      sizeof(out_result->message),
                      "hash-chain verification unavailable until UEFI persistent SQLite audit exists");
    return false;
}
#endif

void audit_show_hash_chain_report(void) {
    vita_audit_hash_chain_result_t result;
    char num[32];

    (void)audit_verify_hash_chain(&result);

    console_write_line("Audit hash-chain verification / Verificacion de cadena hash:");
    console_write_line(result.available ? "available: yes" : "available: no");
    console_write_line(result.ok ? "result: OK" : "result: NOT-OK");

    console_write_line("checked_events:");
    audit_u64_to_dec(result.checked_events, num);
    console_write_line(num);

    console_write_line("first_seq:");
    audit_u64_to_dec(result.first_seq, num);
    console_write_line(num);

    console_write_line("last_seq:");
    audit_u64_to_dec(result.last_seq, num);
    console_write_line(num);

    console_write_line("first_bad_event:");
    console_write_line(result.first_bad_event[0] ? result.first_bad_event : "none");

    console_write_line("message:");
    console_write_line(result.message[0] ? result.message : "none");
}

/* VITA_PATCH_V26_AUDIT_HASH_CHAIN_END */
CADD
fi

# Command dispatch for audit verification aliases.
if ! grep -q 'audit_show_hash_chain_report' "$COMMAND_C"; then
  perl -0pi -e 's/(    if \(str_eq\(cmd, "audit"\)[\s\S]*?return VITA_COMMAND_CONTINUE;\n    }\n)/$1\n    if (str_eq(cmd, "audit verify") ||\n        str_eq(cmd, "audit verify-chain") ||\n        str_eq(cmd, "audit hash") ||\n        str_eq(cmd, "audit hash-chain")) {\n        audit_emit_boot_event("COMMAND_AUDIT_VERIFY", "audit verify");\n        audit_show_hash_chain_report();\n        return VITA_COMMAND_CONTINUE;\n    }\n/' "$COMMAND_C"
fi

cat > "$DOC" <<'DOC'
# Audit hash-chain verification

Patch: `audit: add hash chain verification command`

## Commands

```text
audit verify
audit verify-chain
audit hash
audit hash-chain
```

## Hosted behavior

The verifier checks `audit_event` rows for the active boot session only.

It validates:

1. the `prev_hash` link for each event;
2. the recomputed SHA-256 `event_hash`.

The current boot is used intentionally because each boot starts a new in-memory hash chain.

## UEFI behavior

UEFI reports that verification is unavailable until persistent SQLite audit exists in the freestanding path.

## Scope

This command reports verification status only. It does not repair, rewrite, delete or export audit rows.
DOC

# Basic sanity checks.
if ! grep -q 'vita_audit_hash_chain_result_t' "$AUDIT_H"; then
  echo "[v26] ERROR: audit.h missing hash-chain result type" >&2
  exit 1
fi

if ! grep -q 'audit_verify_hash_chain' "$AUDIT_C"; then
  echo "[v26] ERROR: audit_core.c missing audit_verify_hash_chain" >&2
  exit 1
fi

if ! grep -q 'audit hash-chain' "$COMMAND_C"; then
  echo "[v26] ERROR: command_core.c missing audit hash-chain command alias" >&2
  exit 1
fi

if grep -RInE '(^#!.*python|python3|[^A-Za-z0-9_]python[^A-Za-z0-9_])' "$AUDIT_H" "$AUDIT_C" "$COMMAND_C" "$DOC" 2>/dev/null; then
  echo "[v26] ERROR: unexpected Python reference in v26 files" >&2
  exit 1
fi

echo "[v26] audit hash-chain verification patch applied"
