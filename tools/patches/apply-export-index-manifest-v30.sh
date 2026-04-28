#!/usr/bin/env bash
set -euo pipefail

# VitaOS patch v30: export index manifest.
# Bash/perl only. No Python, no malloc, no GUI/window manager.

ROOT=$(pwd)
MAKEFILE="$ROOT/Makefile"
COMMAND_C="$ROOT/kernel/command_core.c"
HEADER="$ROOT/include/vita/export_manifest.h"
SOURCE="$ROOT/kernel/export_manifest.c"
DOC_DIR="$ROOT/docs/export"
DOC="$DOC_DIR/export-index-manifest.md"

if [[ ! -f "$MAKEFILE" || ! -f "$COMMAND_C" || ! -d "$ROOT/kernel" || ! -d "$ROOT/include/vita" ]]; then
  echo "[v30] ERROR: run this from the VitaOS repository root" >&2
  exit 1
fi

mkdir -p "$DOC_DIR"

cat > "$HEADER" <<'HADD'
#ifndef VITA_EXPORT_MANIFEST_H
#define VITA_EXPORT_MANIFEST_H

#include <stdbool.h>

bool export_manifest_write(void);
void export_manifest_show(void);
bool export_manifest_handle_command(const char *cmd);

#endif
HADD

cat > "$SOURCE" <<'CADD'
/*
 * kernel/export_manifest.c
 * Bounded export index manifest for VitaOS F1A/F1B.
 *
 * This is not a filesystem browser. It writes a small index of known VitaOS
 * export/journal files so the operator can inspect generated reports from USB.
 */

#include <stdint.h>

#include <vita/console.h>
#include <vita/export_manifest.h>
#include <vita/storage.h>

#define EXPORT_INDEX_TEXT_PATH  "/vita/export/export-index.txt"
#define EXPORT_INDEX_JSONL_PATH "/vita/export/export-index.jsonl"
#define EXPORT_MANIFEST_BUFFER_MAX 4096U
#define EXPORT_MANIFEST_PREVIEW_MAX 2048U

typedef struct {
    const char *kind;
    const char *path;
    const char *description;
} export_manifest_entry_t;

static const export_manifest_entry_t g_export_entries[] = {
    {"report",  "/vita/export/reports/last-session.txt", "plain text last-session report"},
    {"report",  "/vita/export/reports/last-session.jsonl", "JSONL last-session report"},
    {"report",  "/vita/export/reports/diagnostic-bundle.txt", "plain text diagnostic bundle"},
    {"report",  "/vita/export/reports/diagnostic-bundle.jsonl", "JSONL diagnostic bundle"},
    {"audit",   "/vita/export/audit/audit-verify.txt", "plain text audit verification export"},
    {"audit",   "/vita/export/audit/audit-verify.jsonl", "JSONL audit verification export"},
    {"audit",   "/vita/export/audit/current-session-events.txt", "plain text current session audit events"},
    {"audit",   "/vita/export/audit/current-session-events.jsonl", "JSONL current session audit events"},
    {"journal", "/vita/audit/session-journal.txt", "plain text visible session journal"},
    {"journal", "/vita/audit/session-journal.jsonl", "JSONL visible session journal"}
};

static unsigned long str_len(const char *s) {
    unsigned long n = 0;

    if (!s) {
        return 0;
    }

    while (s[n]) {
        n++;
    }

    return n;
}

static bool str_eq(const char *a, const char *b) {
    unsigned long i = 0;

    if (!a || !b) {
        return false;
    }

    while (a[i] && b[i]) {
        if (a[i] != b[i]) {
            return false;
        }
        i++;
    }

    return a[i] == b[i];
}

static void zero_buffer(char *buf, unsigned long cap) {
    unsigned long i;

    if (!buf || cap == 0U) {
        return;
    }

    for (i = 0; i < cap; ++i) {
        buf[i] = '\0';
    }
}

static void append_text(char *buf, unsigned long cap, unsigned long *len, const char *text) {
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

static void append_line(char *buf, unsigned long cap, unsigned long *len, const char *text) {
    append_text(buf, cap, len, text ? text : "");
    append_text(buf, cap, len, "\n");
}

static void append_json_string(char *buf, unsigned long cap, unsigned long *len, const char *text) {
    unsigned long i;

    if (!text) {
        return;
    }

    for (i = 0; text[i]; ++i) {
        char c = text[i];

        if (c == '"') {
            append_text(buf, cap, len, "\\\"");
        } else if (c == '\\') {
            append_text(buf, cap, len, "\\\\");
        } else if (c == '\n' || c == '\r' || c == '\t') {
            append_text(buf, cap, len, " ");
        } else if ((unsigned char)c < 32U) {
            append_text(buf, cap, len, "?");
        } else {
            char one[2];
            one[0] = c;
            one[1] = '\0';
            append_text(buf, cap, len, one);
        }
    }
}

static void u64_to_dec(uint64_t value, char out[32]) {
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

static void append_kv(char *buf,
                      unsigned long cap,
                      unsigned long *len,
                      const char *key,
                      const char *value) {
    append_text(buf, cap, len, key ? key : "key");
    append_text(buf, cap, len, ": ");
    append_line(buf, cap, len, value ? value : "");
}

static bool read_known_file(const char *path, char *preview, unsigned long preview_cap, unsigned long *bytes) {
    bool ok;

    if (bytes) {
        *bytes = 0U;
    }

    if (!preview || preview_cap == 0U) {
        return false;
    }

    preview[0] = '\0';
    ok = storage_read_text(path, preview, preview_cap);
    if (ok && bytes) {
        *bytes = str_len(preview);
    }

    return ok;
}

bool export_manifest_write(void) {
    static char text[EXPORT_MANIFEST_BUFFER_MAX];
    static char jsonl[EXPORT_MANIFEST_BUFFER_MAX];
    static char preview[EXPORT_MANIFEST_PREVIEW_MAX];
    unsigned long text_len = 0U;
    unsigned long json_len = 0U;
    unsigned long i;
    unsigned long present_count = 0U;
    unsigned long missing_count = 0U;
    bool ok_text;
    bool ok_jsonl;
    char num[32];

    zero_buffer(text, sizeof(text));
    zero_buffer(jsonl, sizeof(jsonl));

    append_line(text, sizeof(text), &text_len, "VitaOS export index manifest");
    append_line(text, sizeof(text), &text_len, "Indice de exports VitaOS");
    append_line(text, sizeof(text), &text_len, "");
    append_kv(text, sizeof(text), &text_len, "text_path", EXPORT_INDEX_TEXT_PATH);
    append_kv(text, sizeof(text), &text_len, "jsonl_path", EXPORT_INDEX_JSONL_PATH);
    append_line(text, sizeof(text), &text_len, "");
    append_line(text, sizeof(text), &text_len, "Known export/journal files:");

    append_text(jsonl, sizeof(jsonl), &json_len, "{\"type\":\"export_index_manifest\",\"text_path\":\"");
    append_json_string(jsonl, sizeof(jsonl), &json_len, EXPORT_INDEX_TEXT_PATH);
    append_text(jsonl, sizeof(jsonl), &json_len, "\",\"jsonl_path\":\"");
    append_json_string(jsonl, sizeof(jsonl), &json_len, EXPORT_INDEX_JSONL_PATH);
    append_text(jsonl, sizeof(jsonl), &json_len, "\"}\n");

    for (i = 0U; i < (sizeof(g_export_entries) / sizeof(g_export_entries[0])); ++i) {
        unsigned long bytes = 0U;
        bool present = read_known_file(g_export_entries[i].path, preview, sizeof(preview), &bytes);

        if (present) {
            present_count++;
        } else {
            missing_count++;
        }

        append_text(text, sizeof(text), &text_len, present ? "[present] " : "[missing] ");
        append_text(text, sizeof(text), &text_len, g_export_entries[i].kind);
        append_text(text, sizeof(text), &text_len, " ");
        append_line(text, sizeof(text), &text_len, g_export_entries[i].path);
        append_text(text, sizeof(text), &text_len, "  description: ");
        append_line(text, sizeof(text), &text_len, g_export_entries[i].description);
        append_text(text, sizeof(text), &text_len, "  preview_bytes: ");
        u64_to_dec((uint64_t)bytes, num);
        append_line(text, sizeof(text), &text_len, num);

        append_text(jsonl, sizeof(jsonl), &json_len, "{\"type\":\"export_entry\",\"kind\":\"");
        append_json_string(jsonl, sizeof(jsonl), &json_len, g_export_entries[i].kind);
        append_text(jsonl, sizeof(jsonl), &json_len, "\",\"path\":\"");
        append_json_string(jsonl, sizeof(jsonl), &json_len, g_export_entries[i].path);
        append_text(jsonl, sizeof(jsonl), &json_len, "\",\"description\":\"");
        append_json_string(jsonl, sizeof(jsonl), &json_len, g_export_entries[i].description);
        append_text(jsonl, sizeof(jsonl), &json_len, "\",\"present\":");
        append_text(jsonl, sizeof(jsonl), &json_len, present ? "true" : "false");
        append_text(jsonl, sizeof(jsonl), &json_len, ",\"preview_bytes\":");
        append_text(jsonl, sizeof(jsonl), &json_len, num);
        append_text(jsonl, sizeof(jsonl), &json_len, "}\n");
    }

    append_line(text, sizeof(text), &text_len, "");
    u64_to_dec((uint64_t)present_count, num);
    append_kv(text, sizeof(text), &text_len, "present_count", num);
    u64_to_dec((uint64_t)missing_count, num);
    append_kv(text, sizeof(text), &text_len, "missing_count", num);
    append_line(text, sizeof(text), &text_len, "");
    append_line(text, sizeof(text), &text_len, "Scope: bounded known-path manifest only; this is not a recursive file browser.");

    ok_text = storage_write_text(EXPORT_INDEX_TEXT_PATH, text);
    ok_jsonl = storage_write_text(EXPORT_INDEX_JSONL_PATH, jsonl);

    return ok_text && ok_jsonl;
}

void export_manifest_show(void) {
    console_write_line("Export index manifest / Indice de exports:");

    if (export_manifest_write()) {
        console_write_line("export index: ok");
        console_write_line(EXPORT_INDEX_TEXT_PATH);
        console_write_line(EXPORT_INDEX_JSONL_PATH);
        return;
    }

    console_write_line("export index: failed");
    console_write_line("Run storage repair, then retry export index.");
    console_write_line("Ejecuta storage repair y vuelve a intentar export index.");
}

bool export_manifest_handle_command(const char *cmd) {
    if (!cmd) {
        return false;
    }

    if (str_eq(cmd, "export index") ||
        str_eq(cmd, "export manifest") ||
        str_eq(cmd, "export list") ||
        str_eq(cmd, "exports") ||
        str_eq(cmd, "storage export-index")) {
        export_manifest_show();
        return true;
    }

    return false;
}
CADD

if ! grep -q 'kernel/export_manifest.c' "$MAKEFILE"; then
  perl -0pi -e 's/(\tkernel\/console\.c \\\n)/$1\tkernel\/export_manifest.c \\\n/' "$MAKEFILE"
fi

if ! grep -q '#include <vita/export_manifest.h>' "$COMMAND_C"; then
  perl -0pi -e 's/#include <vita\/command.h>\n/#include <vita\/command.h>\n#include <vita\/export_manifest.h>\n/' "$COMMAND_C"
fi

if ! grep -q 'export_manifest_handle_command' "$COMMAND_C"; then
  perl -0pi -e 's/(    if \(str_eq\(cmd, "shutdown"\) \|\| str_eq\(cmd, "exit"\)\) \{)/    if (export_manifest_handle_command(cmd)) {\n        audit_emit_boot_event("COMMAND_EXPORT_INDEX", "export index");\n        return VITA_COMMAND_CONTINUE;\n    }\n\n$1/' "$COMMAND_C"
fi

cat > "$DOC" <<'DOC'
# Export index manifest

Patch: `storage: add export index manifest`

## Purpose

VitaOS now has several local export files under `/vita/export/` and visible journal files under `/vita/audit/`.

This patch adds a bounded manifest command so the operator can create and inspect an index of known export files without remembering every path.

## Commands

```text
export index
export manifest
export list
exports
storage export-index
```

## Output files

```text
/vita/export/export-index.txt
/vita/export/export-index.jsonl
```

## Behavior

The manifest checks known VitaOS report paths and records whether each one is currently readable.

It does not recursively scan the disk and does not expose a broad filesystem browser.

## Known indexed paths

```text
/vita/export/reports/last-session.txt
/vita/export/reports/last-session.jsonl
/vita/export/reports/diagnostic-bundle.txt
/vita/export/reports/diagnostic-bundle.jsonl
/vita/export/audit/audit-verify.txt
/vita/export/audit/audit-verify.jsonl
/vita/export/audit/current-session-events.txt
/vita/export/audit/current-session-events.jsonl
/vita/audit/session-journal.txt
/vita/audit/session-journal.jsonl
```

## Scope boundaries

Not included:

- recursive filesystem browser;
- wildcard scans;
- compression/encryption;
- remote upload;
- Python;
- malloc;
- GUI/window manager;
- networking or Wi-Fi expansion.
DOC

for sym in \
  'export_manifest_write' \
  'export_manifest_show' \
  'export_manifest_handle_command' \
  'EXPORT_INDEX_TEXT_PATH' \
  'current-session-events.jsonl'; do
  if ! grep -Rqs "$sym" "$HEADER" "$SOURCE" "$COMMAND_C" "$DOC" 2>/dev/null; then
    echo "[v30] ERROR: missing expected symbol/text: $sym" >&2
    exit 1
  fi
done

if ! grep -q 'kernel/export_manifest.c' "$MAKEFILE"; then
  echo "[v30] ERROR: Makefile does not include kernel/export_manifest.c" >&2
  exit 1
fi

if grep -RInE '(^#!.*python|python3|[^A-Za-z0-9_]python[^A-Za-z0-9_])' \
  "$HEADER" "$SOURCE" "$COMMAND_C" "$DOC" 2>/dev/null; then
  echo "[v30] ERROR: unexpected Python reference in v30 files" >&2
  exit 1
fi

echo "[v30] export index manifest patch applied"
