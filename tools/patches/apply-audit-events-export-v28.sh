#!/usr/bin/env bash
set -euo pipefail

# VitaOS patch v28: export current session audit events.
# Bash/perl only. No Python, no malloc, no GUI/window manager.

ROOT=$(pwd)
AUDIT_H="$ROOT/include/vita/audit.h"
AUDIT_C="$ROOT/kernel/audit_core.c"
COMMAND_C="$ROOT/kernel/command_core.c"
DOC_DIR="$ROOT/docs/audit"
DOC="$DOC_DIR/audit-current-session-events-export.md"

if [[ ! -f "$AUDIT_H" || ! -f "$AUDIT_C" || ! -f "$COMMAND_C" || ! -f "$ROOT/Makefile" ]]; then
  echo "[v28] ERROR: run this from the VitaOS repository root" >&2
  exit 1
fi

mkdir -p "$DOC_DIR"

# audit_core needs storage and console for export/report helpers.
if ! grep -q '#include <vita/storage.h>' "$AUDIT_C"; then
  perl -0pi -e 's/#include <vita\/audit.h>\n/#include <vita\/audit.h>\n#include <vita\/storage.h>\n/' "$AUDIT_C"
fi

if ! grep -q '#include <vita/console.h>' "$AUDIT_C"; then
  perl -0pi -e 's/#include <vita\/audit.h>\n/#include <vita\/audit.h>\n#include <vita\/console.h>\n/' "$AUDIT_C"
fi

# Public API for current-session event export.
if ! grep -q 'audit_export_current_session_events' "$AUDIT_H"; then
  perl -0pi -e 's/(bool audit_export_recent_event_block\(char \*out, unsigned long out_cap\);\n)/$1bool audit_export_current_session_events(void);\nvoid audit_show_current_session_events_export(void);\n/' "$AUDIT_H"
fi

# Implementation: report-only export of current boot audit_event rows.
if ! grep -q 'VITA_PATCH_V28_AUDIT_EVENTS_EXPORT_START' "$AUDIT_C"; then
  cat >> "$AUDIT_C" <<'CADD'

/* VITA_PATCH_V28_AUDIT_EVENTS_EXPORT_START */

#define AUDIT_EVENTS_TEXT_EXPORT_PATH  "/vita/export/audit/current-session-events.txt"
#define AUDIT_EVENTS_JSONL_EXPORT_PATH "/vita/export/audit/current-session-events.jsonl"
#define AUDIT_EVENTS_TEXT_MAX 8192U
#define AUDIT_EVENTS_JSONL_MAX 8192U
#define AUDIT_EVENTS_FIELD_MAX 256U

static void audit_events_export_zero(char *buf, unsigned long cap) {
    unsigned long i;

    if (!buf || cap == 0U) {
        return;
    }

    for (i = 0; i < cap; ++i) {
        buf[i] = '\0';
    }
}

static void audit_events_export_append(char *buf,
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

static void audit_events_export_append_line(char *buf,
                                            unsigned long cap,
                                            unsigned long *len,
                                            const char *text) {
    audit_events_export_append(buf, cap, len, text ? text : "");
    audit_events_export_append(buf, cap, len, "\n");
}

static void audit_events_export_u64_to_dec(uint64_t value, char out[32]) {
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

static void audit_events_export_copy_field(char *dst,
                                           unsigned long cap,
                                           const unsigned char *src) {
    unsigned long i = 0U;

    if (!dst || cap == 0U) {
        return;
    }

    if (!src) {
        dst[0] = '\0';
        return;
    }

    while (src[i] && (i + 1U) < cap) {
        unsigned char c = src[i];
        if (c == '\r' || c == '\n' || c == '\t') {
            dst[i] = ' ';
        } else if (c >= 32U && c < 127U) {
            dst[i] = (char)c;
        } else {
            dst[i] = '?';
        }
        i++;
    }

    dst[i] = '\0';
}

static void audit_events_export_json_string(char *buf,
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
            audit_events_export_append(buf, cap, len, "\\\"");
        } else if (c == '\\') {
            audit_events_export_append(buf, cap, len, "\\\\");
        } else if (c == '\r' || c == '\n' || c == '\t') {
            audit_events_export_append(buf, cap, len, " ");
        } else if ((unsigned char)c < 32U) {
            audit_events_export_append(buf, cap, len, "?");
        } else {
            char one[2];
            one[0] = c;
            one[1] = '\0';
            audit_events_export_append(buf, cap, len, one);
        }
    }
}

#ifdef VITA_HOSTED
static bool audit_events_export_hosted(char *text,
                                       unsigned long text_cap,
                                       char *jsonl,
                                       unsigned long jsonl_cap,
                                       uint64_t *out_count) {
    const char *sql =
        "SELECT event_seq,event_type,severity,actor_type,summary,prev_hash,event_hash "
        "FROM audit_event "
        "WHERE boot_id=?1 "
        "ORDER BY event_seq ASC,id ASC;";
    sqlite3_stmt *st = NULL;
    unsigned long text_len = 0U;
    unsigned long json_len = 0U;
    uint64_t count = 0U;
    int rc;

    if (!text || !jsonl || !out_count || !g_audit.persistent_ready || !g_audit.db || !g_audit.boot_id[0]) {
        return false;
    }

    *out_count = 0U;
    audit_events_export_zero(text, text_cap);
    audit_events_export_zero(jsonl, jsonl_cap);

    if (sqlite3_prepare_v2(g_audit.db, sql, -1, &st, NULL) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(st, 1, g_audit.boot_id, -1, SQLITE_STATIC);

    audit_events_export_append_line(text, text_cap, &text_len, "VitaOS current session audit events");
    audit_events_export_append_line(text, text_cap, &text_len, "Eventos de auditoria de la sesion actual");
    audit_events_export_append(text, text_cap, &text_len, "boot_id: ");
    audit_events_export_append_line(text, text_cap, &text_len, g_audit.boot_id);
    audit_events_export_append_line(text, text_cap, &text_len, "");

    while ((rc = sqlite3_step(st)) == SQLITE_ROW) {
        char seq[32];
        char event_type[AUDIT_EVENTS_FIELD_MAX];
        char severity[AUDIT_EVENTS_FIELD_MAX];
        char actor_type[AUDIT_EVENTS_FIELD_MAX];
        char summary[AUDIT_EVENTS_FIELD_MAX];
        char prev_hash[AUDIT_EVENTS_FIELD_MAX];
        char event_hash[AUDIT_EVENTS_FIELD_MAX];
        uint64_t event_seq = (uint64_t)sqlite3_column_int64(st, 0);

        audit_events_export_u64_to_dec(event_seq, seq);
        audit_events_export_copy_field(event_type, sizeof(event_type), sqlite3_column_text(st, 1));
        audit_events_export_copy_field(severity, sizeof(severity), sqlite3_column_text(st, 2));
        audit_events_export_copy_field(actor_type, sizeof(actor_type), sqlite3_column_text(st, 3));
        audit_events_export_copy_field(summary, sizeof(summary), sqlite3_column_text(st, 4));
        audit_events_export_copy_field(prev_hash, sizeof(prev_hash), sqlite3_column_text(st, 5));
        audit_events_export_copy_field(event_hash, sizeof(event_hash), sqlite3_column_text(st, 6));

        audit_events_export_append(text, text_cap, &text_len, "#");
        audit_events_export_append(text, text_cap, &text_len, seq);
        audit_events_export_append(text, text_cap, &text_len, " ");
        audit_events_export_append(text, text_cap, &text_len, event_type[0] ? event_type : "UNSPECIFIED");
        audit_events_export_append(text, text_cap, &text_len, " [");
        audit_events_export_append(text, text_cap, &text_len, severity[0] ? severity : "INFO");
        audit_events_export_append(text, text_cap, &text_len, "] ");
        audit_events_export_append_line(text, text_cap, &text_len, summary[0] ? summary : "");
        audit_events_export_append(text, text_cap, &text_len, "  actor_type: ");
        audit_events_export_append_line(text, text_cap, &text_len, actor_type[0] ? actor_type : "SYSTEM");
        audit_events_export_append(text, text_cap, &text_len, "  prev_hash: ");
        audit_events_export_append_line(text, text_cap, &text_len, prev_hash[0] ? prev_hash : "");
        audit_events_export_append(text, text_cap, &text_len, "  event_hash: ");
        audit_events_export_append_line(text, text_cap, &text_len, event_hash[0] ? event_hash : "");
        audit_events_export_append_line(text, text_cap, &text_len, "");

        audit_events_export_append(jsonl, jsonl_cap, &json_len, "{\"type\":\"audit_event\",\"boot_id\":\"");
        audit_events_export_json_string(jsonl, jsonl_cap, &json_len, g_audit.boot_id);
        audit_events_export_append(jsonl, jsonl_cap, &json_len, "\",\"event_seq\":");
        audit_events_export_append(jsonl, jsonl_cap, &json_len, seq);
        audit_events_export_append(jsonl, jsonl_cap, &json_len, ",\"event_type\":\"");
        audit_events_export_json_string(jsonl, jsonl_cap, &json_len, event_type);
        audit_events_export_append(jsonl, jsonl_cap, &json_len, "\",\"severity\":\"");
        audit_events_export_json_string(jsonl, jsonl_cap, &json_len, severity);
        audit_events_export_append(jsonl, jsonl_cap, &json_len, "\",\"actor_type\":\"");
        audit_events_export_json_string(jsonl, jsonl_cap, &json_len, actor_type);
        audit_events_export_append(jsonl, jsonl_cap, &json_len, "\",\"summary\":\"");
        audit_events_export_json_string(jsonl, jsonl_cap, &json_len, summary);
        audit_events_export_append(jsonl, jsonl_cap, &json_len, "\",\"prev_hash\":\"");
        audit_events_export_json_string(jsonl, jsonl_cap, &json_len, prev_hash);
        audit_events_export_append(jsonl, jsonl_cap, &json_len, "\",\"event_hash\":\"");
        audit_events_export_json_string(jsonl, jsonl_cap, &json_len, event_hash);
        audit_events_export_append(jsonl, jsonl_cap, &json_len, "\"}\n");

        count++;
    }

    sqlite3_finalize(st);

    *out_count = count;
    return rc == SQLITE_DONE;
}
#endif

bool audit_export_current_session_events(void) {
    static char text[AUDIT_EVENTS_TEXT_MAX];
    static char jsonl[AUDIT_EVENTS_JSONL_MAX];
    uint64_t count = 0U;
    bool built = false;

#ifdef VITA_HOSTED
    built = audit_events_export_hosted(text, sizeof(text), jsonl, sizeof(jsonl), &count);
#else
    unsigned long text_len = 0U;
    unsigned long json_len = 0U;

    audit_events_export_zero(text, sizeof(text));
    audit_events_export_zero(jsonl, sizeof(jsonl));

    audit_events_export_append_line(text, sizeof(text), &text_len, "VitaOS current session audit events");
    audit_events_export_append_line(text, sizeof(text), &text_len, "Export unavailable in UEFI restricted diagnostic mode.");
    audit_events_export_append_line(text, sizeof(text), &text_len, "Persistent SQLite audit is not available in the freestanding path yet.");

    audit_events_export_append(jsonl, sizeof(jsonl), &json_len, "{\"type\":\"audit_events_export\",\"available\":false,\"message\":\"UEFI persistent SQLite audit unavailable\"}\n");
    built = true;
#endif

    if (!built) {
        unsigned long text_len = 0U;
        unsigned long json_len = 0U;
        audit_events_export_zero(text, sizeof(text));
        audit_events_export_zero(jsonl, sizeof(jsonl));
        audit_events_export_append_line(text, sizeof(text), &text_len, "VitaOS current session audit events");
        audit_events_export_append_line(text, sizeof(text), &text_len, "export failed: audit backend unavailable or query failed");
        audit_events_export_append(jsonl, sizeof(jsonl), &json_len, "{\"type\":\"audit_events_export\",\"available\":false,\"message\":\"audit backend unavailable or query failed\"}\n");
    } else {
        char count_buf[32];
        audit_events_export_u64_to_dec(count, count_buf);
        (void)count_buf;
    }

    return storage_write_text(AUDIT_EVENTS_TEXT_EXPORT_PATH, text) &&
           storage_write_text(AUDIT_EVENTS_JSONL_EXPORT_PATH, jsonl);
}

void audit_show_current_session_events_export(void) {
    console_write_line("Audit events export / Exportacion de eventos de auditoria:");

    if (audit_export_current_session_events()) {
        console_write_line("export: ok");
        console_write_line(AUDIT_EVENTS_TEXT_EXPORT_PATH);
        console_write_line(AUDIT_EVENTS_JSONL_EXPORT_PATH);
        return;
    }

    console_write_line("export: failed");
    console_write_line("Check storage repair/status and the /vita/export/audit tree.");
    console_write_line("Revisa storage repair/status y el arbol /vita/export/audit.");
}

/* VITA_PATCH_V28_AUDIT_EVENTS_EXPORT_END */
CADD
fi

# Command dispatch for current-session audit event export aliases.
if ! grep -q 'audit_show_current_session_events_export' "$COMMAND_C"; then
  perl -0pi -e 's/(    if \(str_eq\(cmd, "audit"\) \|\|)/    if (str_eq(cmd, "audit events") ||\n        str_eq(cmd, "audit export events") ||\n        str_eq(cmd, "audit events export") ||\n        str_eq(cmd, "export audit events") ||\n        str_eq(cmd, "export current audit")) {\n        audit_emit_boot_event("COMMAND_AUDIT_EVENTS_EXPORT", "audit export events");\n        audit_show_current_session_events_export();\n        return VITA_COMMAND_CONTINUE;\n    }\n\n$1/' "$COMMAND_C"
fi

cat > "$DOC" <<'DOC'
# Current session audit events export

Patch: `audit: export current session events`

## Purpose

This patch exports the current boot session's `audit_event` rows into the prepared VitaOS writable tree.

It complements:

- `audit verify` / `audit hash-chain`, which verifies the current session hash chain;
- `audit export`, which exports the verification result;
- `diagnostic`, which exports a high-level system bundle.

## Commands

```text
audit events
audit export events
audit events export
export audit events
export current audit
```

## Output files

```text
/vita/export/audit/current-session-events.txt
/vita/export/audit/current-session-events.jsonl
```

## Hosted behavior

Hosted mode exports the active boot session's SQLite `audit_event` rows ordered by `event_seq`.

## UEFI behavior

UEFI writes an honest unavailable report until the freestanding path has real persistent SQLite audit.

## Scope boundaries

This patch is report/export only.

Not included:

- UEFI SQLite persistence;
- full historical multi-boot export;
- audit row repair or mutation;
- compression or encryption;
- remote upload;
- Python runtime;
- malloc;
- GUI/window manager;
- networking or Wi-Fi expansion.
DOC

# Basic sanity checks.
for sym in \
  'audit_export_current_session_events' \
  'audit_show_current_session_events_export' \
  'AUDIT_EVENTS_TEXT_EXPORT_PATH' \
  'AUDIT_EVENTS_JSONL_EXPORT_PATH'; do
  if ! grep -q "$sym" "$AUDIT_H" "$AUDIT_C"; then
    echo "[v28] ERROR: missing expected symbol: $sym" >&2
    exit 1
  fi
done

if ! grep -q 'export audit events' "$COMMAND_C"; then
  echo "[v28] ERROR: command_core.c missing export audit events alias" >&2
  exit 1
fi

if grep -RInE '(^#!.*python|python3|[^A-Za-z0-9_]python[^A-Za-z0-9_])' "$AUDIT_H" "$AUDIT_C" "$COMMAND_C" "$DOC" 2>/dev/null; then
  echo "[v28] ERROR: unexpected Python reference in v28 files" >&2
  exit 1
fi

echo "[v28] current session audit events export patch applied"
