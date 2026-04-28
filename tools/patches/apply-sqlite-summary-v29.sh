#!/usr/bin/env bash
set -euo pipefail

# VitaOS patch v29: hosted SQLite audit summary command.
# Bash/perl only. No Python, no malloc in UEFI/freestanding path, no GUI/window manager.

ROOT=$(pwd)
AUDIT_H="$ROOT/include/vita/audit.h"
AUDIT_C="$ROOT/kernel/audit_core.c"
COMMAND_C="$ROOT/kernel/command_core.c"
DOC_DIR="$ROOT/docs/audit"
DOC="$DOC_DIR/sqlite-summary.md"

if [[ ! -f "$AUDIT_H" || ! -f "$AUDIT_C" || ! -f "$COMMAND_C" || ! -f "$ROOT/Makefile" ]]; then
  echo "[v29] ERROR: run this from the VitaOS repository root" >&2
  exit 1
fi

mkdir -p "$DOC_DIR"

# audit_core uses console_write_line for the summary report.
if ! grep -q '#include <vita/console.h>' "$AUDIT_C"; then
  perl -0pi -e 's/#include <vita\/audit.h>\n/#include <vita\/audit.h>\n#include <vita\/console.h>\n/' "$AUDIT_C"
fi

# Public summary struct and API.
if ! grep -q 'vita_audit_sqlite_summary_t' "$AUDIT_H"; then
  perl -0pi -e 's/(bool audit_export_recent_event_block\(char \*out, unsigned long out_cap\);\n)/$1\ntypedef struct {\n    bool available;\n    uint64_t boot_sessions;\n    uint64_t audit_events;\n    uint64_t hardware_snapshots;\n    uint64_t ai_proposals;\n    uint64_t human_responses;\n    uint64_t node_peers;\n    uint64_t current_boot_events;\n    char current_boot_id[64];\n    char message[160];\n} vita_audit_sqlite_summary_t;\n\nbool audit_get_sqlite_summary(vita_audit_sqlite_summary_t *out);\nvoid audit_show_sqlite_summary(void);\n/' "$AUDIT_H"
fi

# Implementation.
if ! grep -q 'VITA_PATCH_V29_SQLITE_SUMMARY_START' "$AUDIT_C"; then
  cat >> "$AUDIT_C" <<'CADD'

/* VITA_PATCH_V29_SQLITE_SUMMARY_START */

static void audit_sqlite_summary_zero(void *ptr, unsigned long n) {
    unsigned char *p = (unsigned char *)ptr;
    unsigned long i;

    if (!ptr) {
        return;
    }

    for (i = 0; i < n; ++i) {
        p[i] = 0U;
    }
}

static void audit_sqlite_summary_copy(char *dst, unsigned long cap, const char *src) {
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

static void audit_sqlite_summary_u64_to_dec(uint64_t value, char out[32]) {
    char tmp[32];
    int i = 0;
    int j = 0;

    if (!out) {
        return;
    }

    if (value == 0ULL) {
        out[0] = '0';
        out[1] = '\0';
        return;
    }

    while (value > 0ULL && i < (int)(sizeof(tmp) - 1U)) {
        tmp[i++] = (char)('0' + (value % 10ULL));
        value /= 10ULL;
    }

    while (i > 0) {
        out[j++] = tmp[--i];
    }

    out[j] = '\0';
}

static void audit_sqlite_summary_print_u64(const char *label, uint64_t value) {
    char num[32];

    audit_sqlite_summary_u64_to_dec(value, num);
    console_write_line(label ? label : "value:");
    console_write_line(num);
}

#ifdef VITA_HOSTED
static bool audit_sqlite_summary_count_sql(const char *sql, uint64_t *out_count) {
    sqlite3_stmt *st = NULL;
    bool ok = false;

    if (!sql || !out_count || !g_audit.db) {
        return false;
    }

    *out_count = 0ULL;

    if (sqlite3_prepare_v2(g_audit.db, sql, -1, &st, NULL) != SQLITE_OK) {
        return false;
    }

    if (sqlite3_step(st) == SQLITE_ROW) {
        *out_count = (uint64_t)sqlite3_column_int64(st, 0);
        ok = true;
    }

    sqlite3_finalize(st);
    return ok;
}

static bool audit_sqlite_summary_count_current_boot_events(uint64_t *out_count) {
    sqlite3_stmt *st = NULL;
    bool ok = false;
    const char *sql = "SELECT COUNT(*) FROM audit_event WHERE boot_id = ?1;";

    if (!out_count || !g_audit.db || !g_audit.boot_id[0]) {
        return false;
    }

    *out_count = 0ULL;

    if (sqlite3_prepare_v2(g_audit.db, sql, -1, &st, NULL) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(st, 1, g_audit.boot_id, -1, SQLITE_STATIC);

    if (sqlite3_step(st) == SQLITE_ROW) {
        *out_count = (uint64_t)sqlite3_column_int64(st, 0);
        ok = true;
    }

    sqlite3_finalize(st);
    return ok;
}

bool audit_get_sqlite_summary(vita_audit_sqlite_summary_t *out) {
    bool ok = true;

    if (!out) {
        return false;
    }

    audit_sqlite_summary_zero(out, sizeof(*out));

    if (!g_audit.persistent_ready || !g_audit.db) {
        out->available = false;
        audit_sqlite_summary_copy(out->message, sizeof(out->message), "SQLite audit backend is not ready");
        return false;
    }

    out->available = true;
    audit_sqlite_summary_copy(out->current_boot_id, sizeof(out->current_boot_id), g_audit.boot_id);

    ok = audit_sqlite_summary_count_sql("SELECT COUNT(*) FROM boot_session;", &out->boot_sessions) && ok;
    ok = audit_sqlite_summary_count_sql("SELECT COUNT(*) FROM audit_event;", &out->audit_events) && ok;
    ok = audit_sqlite_summary_count_sql("SELECT COUNT(*) FROM hardware_snapshot;", &out->hardware_snapshots) && ok;
    ok = audit_sqlite_summary_count_sql("SELECT COUNT(*) FROM ai_proposal;", &out->ai_proposals) && ok;
    ok = audit_sqlite_summary_count_sql("SELECT COUNT(*) FROM human_response;", &out->human_responses) && ok;
    ok = audit_sqlite_summary_count_sql("SELECT COUNT(*) FROM node_peer;", &out->node_peers) && ok;
    ok = audit_sqlite_summary_count_current_boot_events(&out->current_boot_events) && ok;

    audit_sqlite_summary_copy(out->message,
                              sizeof(out->message),
                              ok ? "SQLite summary ready" : "SQLite summary partially unavailable");
    return ok;
}
#else
bool audit_get_sqlite_summary(vita_audit_sqlite_summary_t *out) {
    if (!out) {
        return false;
    }

    audit_sqlite_summary_zero(out, sizeof(*out));
    out->available = false;
    audit_sqlite_summary_copy(out->current_boot_id, sizeof(out->current_boot_id), "unavailable");
    audit_sqlite_summary_copy(out->message,
                              sizeof(out->message),
                              "SQLite summary unavailable in UEFI until persistent audit exists");
    return false;
}
#endif

void audit_show_sqlite_summary(void) {
    vita_audit_sqlite_summary_t summary;
    bool ok;

    ok = audit_get_sqlite_summary(&summary);

    console_write_line("SQLite audit summary / Resumen SQLite de auditoria:");
    console_write_line(summary.available ? "available: yes" : "available: no");
    console_write_line(ok ? "status: ok" : "status: not-ok");
    console_write_line("current_boot_id:");
    console_write_line(summary.current_boot_id[0] ? summary.current_boot_id : "none");

    audit_sqlite_summary_print_u64("boot_sessions:", summary.boot_sessions);
    audit_sqlite_summary_print_u64("audit_events:", summary.audit_events);
    audit_sqlite_summary_print_u64("current_boot_events:", summary.current_boot_events);
    audit_sqlite_summary_print_u64("hardware_snapshots:", summary.hardware_snapshots);
    audit_sqlite_summary_print_u64("ai_proposals:", summary.ai_proposals);
    audit_sqlite_summary_print_u64("human_responses:", summary.human_responses);
    audit_sqlite_summary_print_u64("node_peers:", summary.node_peers);

    console_write_line("message:");
    console_write_line(summary.message[0] ? summary.message : "none");

#ifndef VITA_HOSTED
    console_write_line("UEFI note:");
    console_write_line("Persistent SQLite audit is not available yet in the freestanding path.");
#endif
}

/* VITA_PATCH_V29_SQLITE_SUMMARY_END */
CADD
fi

# Command aliases.
if ! grep -q 'audit_show_sqlite_summary' "$COMMAND_C"; then
  perl -0pi -e 's/(    if \(str_eq\(cmd, "peers"\)\) \{)/    if (str_eq(cmd, "audit sqlite") ||\n        str_eq(cmd, "audit sqlite summary") ||\n        str_eq(cmd, "audit db") ||\n        str_eq(cmd, "audit db summary") ||\n        str_eq(cmd, "audit summary db")) {\n        audit_emit_boot_event("COMMAND_AUDIT_SQLITE_SUMMARY", "audit sqlite summary");\n        audit_show_sqlite_summary();\n        return VITA_COMMAND_CONTINUE;\n    }\n\n$1/' "$COMMAND_C"
fi

cat > "$DOC" <<'DOC'
# Hosted SQLite audit summary

Patch: `audit: add hosted SQLite summary command`

## Purpose

This command gives a quick console-readable summary of the hosted SQLite audit backend.

It is intended for F1A/F1B validation, smoke testing and operator review.

## Commands

```text
audit sqlite
audit sqlite summary
audit db
audit db summary
audit summary db
```

## Hosted output

The report includes:

- persistent readiness;
- current boot id;
- total boot sessions;
- total audit events;
- events for the current boot;
- hardware snapshots;
- AI proposals;
- human responses;
- node peers.

## UEFI output

UEFI reports that the SQLite summary is unavailable until the freestanding path has real persistent SQLite audit.

## Scope

This is read-only. It does not modify, repair or delete SQLite rows.
DOC

# Verification.
for sym in \
  'vita_audit_sqlite_summary_t' \
  'audit_get_sqlite_summary' \
  'audit_show_sqlite_summary' \
  'audit sqlite summary'; do
  if ! grep -Rqs "$sym" "$AUDIT_H" "$AUDIT_C" "$COMMAND_C" "$DOC" 2>/dev/null; then
    echo "[v29] ERROR: missing expected symbol: $sym" >&2
    exit 1
  fi
done

if grep -RInE '(^#!.*python|python3|[^A-Za-z0-9_]python[^A-Za-z0-9_])' "$AUDIT_H" "$AUDIT_C" "$COMMAND_C" "$DOC" 2>/dev/null; then
  echo "[v29] ERROR: unexpected Python reference in v29 files" >&2
  exit 1
fi

echo "[v29] hosted SQLite audit summary patch applied"
