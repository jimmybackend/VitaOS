#!/usr/bin/env bash
set -euo pipefail

# VitaOS patch v25: full diagnostic bundle export.
# Bash/perl only. No Python, no malloc, no GUI/window manager.

ROOT=$(pwd)
MAKEFILE="$ROOT/Makefile"
COMMAND_C="$ROOT/kernel/command_core.c"
HEADER_DIR="$ROOT/include/vita"
KERNEL_DIR="$ROOT/kernel"
DOC_DIR="$ROOT/docs/export"
DIAG_H="$HEADER_DIR/diagnostic_bundle.h"
DIAG_C="$KERNEL_DIR/diagnostic_bundle.c"
DOC="$DOC_DIR/diagnostic-bundle.md"

if [[ ! -f "$MAKEFILE" || ! -f "$COMMAND_C" || ! -d "$HEADER_DIR" || ! -d "$KERNEL_DIR" ]]; then
  echo "[v25] ERROR: run this from the VitaOS repository root" >&2
  exit 1
fi

mkdir -p "$DOC_DIR"

cat > "$DIAG_H" <<'H'
#ifndef VITA_DIAGNOSTIC_BUNDLE_H
#define VITA_DIAGNOSTIC_BUNDLE_H

#include <stdbool.h>

#include <vita/command.h>

#define VITA_DIAGNOSTIC_BUNDLE_TEXT_PATH  "/vita/export/reports/diagnostic-bundle.txt"
#define VITA_DIAGNOSTIC_BUNDLE_JSONL_PATH "/vita/export/reports/diagnostic-bundle.jsonl"

bool diagnostic_bundle_handle_command(const char *cmd, const vita_command_context_t *ctx);
bool diagnostic_bundle_export(const vita_command_context_t *ctx);
void diagnostic_bundle_show_help(void);

#endif
H

cat > "$DIAG_C" <<'C'
/*
 * kernel/diagnostic_bundle.c
 * Full diagnostic bundle export for VitaOS F1A/F1B.
 *
 * This is a bounded, text-first report writer. It uses the existing storage
 * facade and does not add a GUI, network upload, Python, malloc, or a full VFS.
 */

#include <stdint.h>

#include <vita/audit.h>
#include <vita/console.h>
#include <vita/diagnostic_bundle.h>
#include <vita/session_journal.h>
#include <vita/storage.h>

#define DIAG_TEXT_MAX 4096U
#define DIAG_JSONL_MAX 4096U

static char g_diag_text[DIAG_TEXT_MAX];
static char g_diag_jsonl[DIAG_JSONL_MAX];
static unsigned long g_diag_text_len;
static unsigned long g_diag_jsonl_len;

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

static void append_buf(char *buf, unsigned long cap, unsigned long *len, const char *text) {
    unsigned long i;

    if (!buf || cap == 0 || !len || !text) {
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

static void append_text(const char *text) {
    append_buf(g_diag_text, sizeof(g_diag_text), &g_diag_text_len, text ? text : "");
}

static void append_text_line(const char *text) {
    append_text(text ? text : "");
    append_text("\n");
}

static void append_json(const char *text) {
    append_buf(g_diag_jsonl, sizeof(g_diag_jsonl), &g_diag_jsonl_len, text ? text : "");
}

static void append_json_escaped(const char *text) {
    unsigned long i;

    if (!text) {
        return;
    }

    for (i = 0; text[i]; ++i) {
        char c = text[i];
        if (c == '"') {
            append_json("\\\"");
        } else if (c == '\\') {
            append_json("\\\\");
        } else if (c == '\n' || c == '\r' || c == '\t') {
            append_json(" ");
        } else if ((unsigned char)c < 32U) {
            append_json("?");
        } else {
            char one[2];
            one[0] = c;
            one[1] = '\0';
            append_json(one);
        }
    }
}

static void u64_to_dec(uint64_t v, char out[32]) {
    char tmp[32];
    int i = 0;
    int j = 0;

    if (!out) {
        return;
    }

    if (v == 0ULL) {
        out[0] = '0';
        out[1] = '\0';
        return;
    }

    while (v > 0ULL && i < (int)(sizeof(tmp) - 1U)) {
        tmp[i++] = (char)('0' + (v % 10ULL));
        v /= 10ULL;
    }

    while (i > 0) {
        out[j++] = tmp[--i];
    }

    out[j] = '\0';
}

static void append_text_kv(const char *key, const char *value) {
    append_text(key ? key : "key");
    append_text(": ");
    append_text_line(value ? value : "");
}

static void append_text_bool(const char *key, bool value) {
    append_text_kv(key, value ? "yes" : "no");
}

static void append_text_u64(const char *key, uint64_t value) {
    char num[32];
    u64_to_dec(value, num);
    append_text_kv(key, num);
}

static void append_json_record_string(const char *type, const char *key, const char *value) {
    append_json("{\"type\":\"");
    append_json_escaped(type ? type : "record");
    append_json("\",\"");
    append_json_escaped(key ? key : "value");
    append_json("\":\"");
    append_json_escaped(value ? value : "");
    append_json("\"}\n");
}

static void append_json_record_bool(const char *type, const char *key, bool value) {
    append_json("{\"type\":\"");
    append_json_escaped(type ? type : "record");
    append_json("\",\"");
    append_json_escaped(key ? key : "value");
    append_json("\":");
    append_json(value ? "true" : "false");
    append_json("}\n");
}

static void append_json_record_u64(const char *type, const char *key, uint64_t value) {
    char num[32];
    u64_to_dec(value, num);
    append_json("{\"type\":\"");
    append_json_escaped(type ? type : "record");
    append_json("\",\"");
    append_json_escaped(key ? key : "value");
    append_json("\":");
    append_json(num);
    append_json("}\n");
}

static const char *firmware_name(const vita_command_context_t *ctx) {
    if (!ctx || !ctx->handoff) {
        return "unknown";
    }

    if (ctx->handoff->firmware_type == VITA_FIRMWARE_HOSTED) {
        return "hosted";
    }

    if (ctx->handoff->firmware_type == VITA_FIRMWARE_UEFI) {
        return "uefi";
    }

    return "unknown";
}

static void reset_buffers(void) {
    g_diag_text_len = 0;
    g_diag_jsonl_len = 0;
    g_diag_text[0] = '\0';
    g_diag_jsonl[0] = '\0';
}

static void append_hw_section(const vita_command_context_t *ctx) {
    const vita_hw_snapshot_t *hw;

    append_text_line("");
    append_text_line("[hardware]");

    if (!ctx || !ctx->hw_ready) {
        append_text_line("hardware_snapshot: unavailable");
        append_json_record_bool("hardware", "snapshot_available", false);
        return;
    }

    hw = &ctx->hw_snapshot;
    append_text_kv("cpu_arch", hw->cpu_arch[0] ? hw->cpu_arch : "unknown");
    append_text_kv("cpu_model", hw->cpu_model[0] ? hw->cpu_model : "unavailable");
    append_text_u64("ram_bytes", hw->ram_bytes);
    append_text_kv("firmware_type", hw->firmware_type[0] ? hw->firmware_type : "unknown");
    append_text_kv("console_type", hw->console_type[0] ? hw->console_type : "unknown");
    append_text_u64("display_count", (uint64_t)((hw->display_count < 0) ? 0 : hw->display_count));
    append_text_u64("keyboard_count", (uint64_t)((hw->keyboard_count < 0) ? 0 : hw->keyboard_count));
    append_text_u64("mouse_count", (uint64_t)((hw->mouse_count < 0) ? 0 : hw->mouse_count));
    append_text_u64("net_count", (uint64_t)((hw->net_count < 0) ? 0 : hw->net_count));
    append_text_u64("ethernet_count", (uint64_t)((hw->ethernet_count < 0) ? 0 : hw->ethernet_count));
    append_text_u64("wifi_count", (uint64_t)((hw->wifi_count < 0) ? 0 : hw->wifi_count));
    append_text_u64("storage_count", (uint64_t)((hw->storage_count < 0) ? 0 : hw->storage_count));
    append_text_u64("usb_count", (uint64_t)((hw->usb_count < 0) ? 0 : hw->usb_count));
    append_text_u64("usb_controller_count", (uint64_t)((hw->usb_controller_count < 0) ? 0 : hw->usb_controller_count));
    append_text_u64("detected_at_unix", hw->detected_at_unix);

    append_json_record_bool("hardware", "snapshot_available", true);
    append_json_record_string("hardware", "cpu_arch", hw->cpu_arch[0] ? hw->cpu_arch : "unknown");
    append_json_record_u64("hardware", "ram_bytes", hw->ram_bytes);
    append_json_record_u64("hardware", "net_count", (uint64_t)((hw->net_count < 0) ? 0 : hw->net_count));
    append_json_record_u64("hardware", "storage_count", (uint64_t)((hw->storage_count < 0) ? 0 : hw->storage_count));
}

static void append_limitations(void) {
    append_text_line("");
    append_text_line("[known_limitations]");
    append_text_line("- UEFI persistent SQLite backend is still staged unless audit readiness reports otherwise.");
    append_text_line("- Diagnostic bundle is local only; no remote upload is attempted.");
    append_text_line("- Bundle is bounded and summary-oriented; it is not a full filesystem dump.");
    append_text_line("- No GUI/window manager, no network expansion, no Wi-Fi driver is added by this patch.");

    append_json_record_string("limitation", "uefi_sqlite", "staged unless audit readiness reports otherwise");
    append_json_record_string("limitation", "upload", "local only");
    append_json_record_string("limitation", "scope", "bounded summary, not full dump");
}

static void build_bundle(const vita_command_context_t *ctx) {
    vita_storage_status_t st;
    bool journal_active;

    reset_buffers();
    storage_get_status(&st);
    journal_active = session_journal_is_active();

    append_text_line("VitaOS diagnostic bundle / Paquete diagnostico VitaOS");
    append_text_line("======================================================");
    append_text_line("");
    append_text_line("[paths]");
    append_text_kv("text", VITA_DIAGNOSTIC_BUNDLE_TEXT_PATH);
    append_text_kv("jsonl", VITA_DIAGNOSTIC_BUNDLE_JSONL_PATH);

    append_text_line("");
    append_text_line("[boot]");
    append_text_kv("firmware_mode", firmware_name(ctx));
    append_text_kv("arch", (ctx && ctx->boot_status.arch_name) ? ctx->boot_status.arch_name : "unknown");
    append_text_bool("console_ready", ctx ? ctx->boot_status.console_ready : false);
    append_text_bool("audit_ready", ctx ? ctx->boot_status.audit_ready : false);
    append_text_bool("proposal_engine_ready", ctx ? ctx->proposal_ready : false);
    append_text_bool("node_core_ready", ctx ? ctx->node_ready : false);

    append_text_line("");
    append_text_line("[storage]");
    append_text_bool("initialized", st.initialized);
    append_text_bool("writable", st.writable);
    append_text_kv("backend", st.backend_name[0] ? st.backend_name : "none");
    append_text_kv("root_hint", st.root_hint[0] ? st.root_hint : "/vita");
    append_text_kv("last_error", st.last_error[0] ? st.last_error : "none");

    append_text_line("");
    append_text_line("[journal]");
    append_text_bool("active", journal_active);
    append_text_line("journal_flush_attempted_before_export: yes");

    append_hw_section(ctx);
    append_limitations();

    append_json_record_string("diagnostic_bundle", "text_path", VITA_DIAGNOSTIC_BUNDLE_TEXT_PATH);
    append_json_record_string("diagnostic_bundle", "jsonl_path", VITA_DIAGNOSTIC_BUNDLE_JSONL_PATH);
    append_json_record_string("boot", "firmware_mode", firmware_name(ctx));
    append_json_record_string("boot", "arch", (ctx && ctx->boot_status.arch_name) ? ctx->boot_status.arch_name : "unknown");
    append_json_record_bool("boot", "console_ready", ctx ? ctx->boot_status.console_ready : false);
    append_json_record_bool("boot", "audit_ready", ctx ? ctx->boot_status.audit_ready : false);
    append_json_record_bool("boot", "proposal_engine_ready", ctx ? ctx->proposal_ready : false);
    append_json_record_bool("boot", "node_core_ready", ctx ? ctx->node_ready : false);
    append_json_record_bool("storage", "initialized", st.initialized);
    append_json_record_bool("storage", "writable", st.writable);
    append_json_record_string("storage", "backend", st.backend_name[0] ? st.backend_name : "none");
    append_json_record_string("storage", "root_hint", st.root_hint[0] ? st.root_hint : "/vita");
    append_json_record_bool("journal", "active", journal_active);
}

bool diagnostic_bundle_export(const vita_command_context_t *ctx) {
    bool ok_text;
    bool ok_jsonl;

    if (session_journal_is_active()) {
        (void)session_journal_flush();
    }

    build_bundle(ctx);

    if (!storage_is_ready()) {
        console_write_line("diagnostic bundle: storage not ready");
        console_write_line("run: storage status");
        return false;
    }

    ok_text = storage_write_text(VITA_DIAGNOSTIC_BUNDLE_TEXT_PATH, g_diag_text);
    ok_jsonl = storage_write_text(VITA_DIAGNOSTIC_BUNDLE_JSONL_PATH, g_diag_jsonl);

    if (ok_text && ok_jsonl) {
        console_write_line("diagnostic bundle: exported");
        console_write_line(VITA_DIAGNOSTIC_BUNDLE_TEXT_PATH);
        console_write_line(VITA_DIAGNOSTIC_BUNDLE_JSONL_PATH);
        audit_emit_boot_event("DIAGNOSTIC_BUNDLE_EXPORTED", VITA_DIAGNOSTIC_BUNDLE_TEXT_PATH);
        return true;
    }

    console_write_line("diagnostic bundle: failed");
    console_write_line("run: storage status");
    audit_emit_boot_event("DIAGNOSTIC_BUNDLE_FAILED", "storage write failed");
    return false;
}

void diagnostic_bundle_show_help(void) {
    console_write_line("Diagnostic bundle commands / Comandos de paquete diagnostico:");
    console_write_line("diagnostic");
    console_write_line("diag");
    console_write_line("diagnostic bundle");
    console_write_line("diagnostic export");
    console_write_line("export diagnostic");
    console_write_line("export diagnostics");
    console_write_line("export bundle");
    console_write_line("export all");
}

bool diagnostic_bundle_handle_command(const char *cmd, const vita_command_context_t *ctx) {
    if (!cmd) {
        return false;
    }

    if (str_eq(cmd, "diagnostic help") || str_eq(cmd, "diag help")) {
        diagnostic_bundle_show_help();
        return true;
    }

    if (str_eq(cmd, "diagnostic") ||
        str_eq(cmd, "diag") ||
        str_eq(cmd, "diagnostic bundle") ||
        str_eq(cmd, "diagnostic export") ||
        str_eq(cmd, "export diagnostic") ||
        str_eq(cmd, "export diagnostics") ||
        str_eq(cmd, "export bundle") ||
        str_eq(cmd, "export all")) {
        (void)diagnostic_bundle_export(ctx);
        return true;
    }

    return false;
}
C

# Add diagnostic bundle module to COMMON_KERNEL.
if ! grep -q 'kernel/diagnostic_bundle.c' "$MAKEFILE"; then
  if grep -q $'\tkernel/command_core.c \\' "$MAKEFILE"; then
    perl -0pi -e 's/(\tkernel\/command_core\.c \\\n)/$1\tkernel\/diagnostic_bundle.c \\\n/' "$MAKEFILE"
  else
    echo "[v25] ERROR: could not find kernel/command_core.c in COMMON_KERNEL" >&2
    exit 1
  fi
fi

# Include the diagnostic bundle command dispatcher.
if ! grep -q '#include <vita/diagnostic_bundle.h>' "$COMMAND_C"; then
  perl -0pi -e 's/(#include <vita\/command\.h>\n)/$1#include <vita\/diagnostic_bundle.h>\n/' "$COMMAND_C"
fi

# Dispatch after command_refresh_state(ctx), before the built-in command list.
if ! grep -q 'diagnostic_bundle_handle_command(cmd, ctx)' "$COMMAND_C"; then
  perl -0pi -e 's/(    command_refresh_state\(ctx\);\n)/$1\n    if (diagnostic_bundle_handle_command(cmd, ctx)) {\n        return VITA_COMMAND_CONTINUE;\n    }\n/' "$COMMAND_C"
fi

cat > "$DOC" <<'DOC'
# Diagnostic bundle export

Patch: `export: add full diagnostic bundle`

## Commands

```text
diagnostic
diag
diagnostic bundle
diagnostic export
export diagnostic
export diagnostics
export bundle
export all
```

## Output files

```text
/vita/export/reports/diagnostic-bundle.txt
/vita/export/reports/diagnostic-bundle.jsonl
```

## Contents

The bundle summarizes:

- boot mode and architecture;
- audit readiness;
- storage backend and root hint;
- journal active state;
- hardware snapshot counts;
- proposal/node readiness;
- known limitations.

## Scope

This is a bounded local report. It is not a full filesystem dump, not a SQLite dump, and not a remote upload.

The patch does not add Python, malloc, GUI/window-manager behavior, new networking, or Wi-Fi support.
DOC

# Sanity checks.
if grep -RInE '(^#!.*python|python3|[^A-Za-z0-9_]python[^A-Za-z0-9_])' "$MAKEFILE" "$COMMAND_C" "$DIAG_C" "$DIAG_H" 2>/dev/null; then
  echo "[v25] ERROR: unexpected Python reference in patched runtime/build files" >&2
  exit 1
fi

for sym in \
  'kernel/diagnostic_bundle.c' \
  'diagnostic_bundle_handle_command' \
  'diagnostic_bundle_export' \
  'VITA_DIAGNOSTIC_BUNDLE_TEXT_PATH' \
  'DIAGNOSTIC_BUNDLE_EXPORTED'; do
  if ! grep -RIn "$sym" "$MAKEFILE" "$COMMAND_C" "$DIAG_C" "$DIAG_H" >/dev/null 2>&1; then
    echo "[v25] ERROR: expected symbol missing after patch: $sym" >&2
    exit 1
  fi
done

echo "[v25] diagnostic bundle export patch applied"
