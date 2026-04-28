#!/usr/bin/env bash
set -euo pipefail

# VitaOS patch v31: boot self-test command.

ROOT=$(pwd)
MAKEFILE="$ROOT/Makefile"
COMMAND_C="$ROOT/kernel/command_core.c"
SELFTEST_C="$ROOT/kernel/selftest.c"
SELFTEST_H="$ROOT/include/vita/selftest.h"
DOC_DIR="$ROOT/docs/boot"
DOC="$DOC_DIR/self-test.md"

if [[ ! -f "$MAKEFILE" || ! -f "$COMMAND_C" || ! -d "$ROOT/kernel" || ! -d "$ROOT/include/vita" ]]; then
  echo "[v31] ERROR: run this from the VitaOS repository root" >&2
  exit 1
fi

mkdir -p "$DOC_DIR"

cat > "$SELFTEST_H" <<'HADD'
#ifndef VITA_SELFTEST_H
#define VITA_SELFTEST_H

#include <stdbool.h>

#include <vita/command.h>

bool vita_selftest_run(const vita_command_context_t *ctx);

#endif
HADD

cat > "$SELFTEST_C" <<'CADD'
/*
 * kernel/selftest.c
 * Bounded boot/session self-test for VitaOS F1A/F1B.
 *
 * This is a local report command. It does not repair audit rows, scan the full
 * filesystem, upload data, or add a GUI/window manager.
 */

#include <stdint.h>

#include <vita/audit.h>
#include <vita/command.h>
#include <vita/console.h>
#include <vita/selftest.h>
#include <vita/session_journal.h>
#include <vita/storage.h>

#define SELFTEST_TEXT_PATH  "/vita/export/reports/self-test.txt"
#define SELFTEST_JSONL_PATH "/vita/export/reports/self-test.jsonl"
#define SELFTEST_TEXT_MAX   2048U
#define SELFTEST_JSON_MAX   1024U

typedef struct {
    bool console_ready;
    bool audit_ready;
    bool storage_ready;
    bool journal_active;
    bool hw_ready;
    bool proposal_ready;
    bool node_ready;
    bool report_written;
    uint32_t pass_count;
    uint32_t warn_count;
    uint32_t fail_count;
} vita_selftest_result_t;

static unsigned long st_strlen(const char *s) {
    unsigned long n = 0;

    if (!s) {
        return 0;
    }

    while (s[n]) {
        n++;
    }

    return n;
}

static void st_zero(char *buf, unsigned long cap) {
    unsigned long i;

    if (!buf || cap == 0U) {
        return;
    }

    for (i = 0; i < cap; ++i) {
        buf[i] = '\0';
    }
}

static void st_append(char *buf, unsigned long cap, unsigned long *len, const char *text) {
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

static void st_append_line(char *buf, unsigned long cap, unsigned long *len, const char *text) {
    st_append(buf, cap, len, text ? text : "");
    st_append(buf, cap, len, "\n");
}

static void st_u32_to_dec(uint32_t value, char out[16]) {
    char tmp[16];
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

static const char *st_yes_no(bool value) {
    return value ? "yes" : "no";
}

static const char *st_pass_fail(bool value) {
    return value ? "PASS" : "FAIL";
}

static void st_count_bool(vita_selftest_result_t *r, bool value) {
    if (!r) {
        return;
    }

    if (value) {
        r->pass_count++;
    } else {
        r->fail_count++;
    }
}

static void st_count_warn(vita_selftest_result_t *r, bool value) {
    if (!r) {
        return;
    }

    if (value) {
        r->pass_count++;
    } else {
        r->warn_count++;
    }
}

static void st_text_kv(char *buf,
                       unsigned long cap,
                       unsigned long *len,
                       const char *key,
                       const char *value) {
    st_append(buf, cap, len, key ? key : "key");
    st_append(buf, cap, len, ": ");
    st_append_line(buf, cap, len, value ? value : "");
}

static void st_json_bool(char *buf,
                         unsigned long cap,
                         unsigned long *len,
                         const char *key,
                         bool value,
                         bool comma) {
    if (comma) {
        st_append(buf, cap, len, ",");
    }
    st_append(buf, cap, len, "\"");
    st_append(buf, cap, len, key ? key : "key");
    st_append(buf, cap, len, "\":");
    st_append(buf, cap, len, value ? "true" : "false");
}

static void st_json_u32(char *buf,
                        unsigned long cap,
                        unsigned long *len,
                        const char *key,
                        uint32_t value,
                        bool comma) {
    char num[16];

    st_u32_to_dec(value, num);
    if (comma) {
        st_append(buf, cap, len, ",");
    }
    st_append(buf, cap, len, "\"");
    st_append(buf, cap, len, key ? key : "key");
    st_append(buf, cap, len, "\":");
    st_append(buf, cap, len, num);
}

static void st_print_line(const char *label, bool value, bool warning_only) {
    console_write_line(label ? label : "check");
    if (warning_only && !value) {
        console_write_line("WARN");
        return;
    }
    console_write_line(st_pass_fail(value));
}

static void st_build_result(const vita_command_context_t *ctx, vita_selftest_result_t *r) {
    if (!r) {
        return;
    }

    r->console_ready = ctx ? ctx->boot_status.console_ready : false;
    r->audit_ready = ctx ? ctx->boot_status.audit_ready : false;
    r->storage_ready = storage_is_ready();
    r->journal_active = session_journal_is_active();
    r->hw_ready = ctx ? ctx->hw_ready : false;
    r->proposal_ready = ctx ? ctx->proposal_ready : false;
    r->node_ready = ctx ? ctx->node_ready : false;
    r->report_written = false;
    r->pass_count = 0U;
    r->warn_count = 0U;
    r->fail_count = 0U;

    st_count_bool(r, r->console_ready);
    st_count_bool(r, r->storage_ready);
    st_count_warn(r, r->audit_ready);
    st_count_warn(r, r->journal_active);
    st_count_warn(r, r->hw_ready);
    st_count_warn(r, r->proposal_ready);
    st_count_warn(r, r->node_ready);
}

static void st_build_reports(const vita_selftest_result_t *r,
                             char *text,
                             unsigned long text_cap,
                             char *json,
                             unsigned long json_cap) {
    unsigned long text_len = 0U;
    unsigned long json_len = 0U;
    char num[16];

    if (!r || !text || !json) {
        return;
    }

    st_zero(text, text_cap);
    st_zero(json, json_cap);

    st_append_line(text, text_cap, &text_len, "VitaOS boot self-test / Autoprueba de arranque VitaOS");
    st_append_line(text, text_cap, &text_len, "");
    st_text_kv(text, text_cap, &text_len, "text_path", SELFTEST_TEXT_PATH);
    st_text_kv(text, text_cap, &text_len, "jsonl_path", SELFTEST_JSONL_PATH);
    st_text_kv(text, text_cap, &text_len, "console_ready", st_yes_no(r->console_ready));
    st_text_kv(text, text_cap, &text_len, "audit_ready", st_yes_no(r->audit_ready));
    st_text_kv(text, text_cap, &text_len, "storage_ready", st_yes_no(r->storage_ready));
    st_text_kv(text, text_cap, &text_len, "journal_active", st_yes_no(r->journal_active));
    st_text_kv(text, text_cap, &text_len, "hw_snapshot_ready", st_yes_no(r->hw_ready));
    st_text_kv(text, text_cap, &text_len, "proposal_engine_ready", st_yes_no(r->proposal_ready));
    st_text_kv(text, text_cap, &text_len, "node_core_ready", st_yes_no(r->node_ready));

    st_u32_to_dec(r->pass_count, num);
    st_text_kv(text, text_cap, &text_len, "pass_count", num);
    st_u32_to_dec(r->warn_count, num);
    st_text_kv(text, text_cap, &text_len, "warn_count", num);
    st_u32_to_dec(r->fail_count, num);
    st_text_kv(text, text_cap, &text_len, "fail_count", num);

    st_append_line(text, text_cap, &text_len, "");
    if (r->fail_count > 0U) {
        st_append_line(text, text_cap, &text_len, "result: FAIL - required checks failed");
    } else if (r->warn_count > 0U) {
        st_append_line(text, text_cap, &text_len, "result: WARN - restricted or partial mode");
    } else {
        st_append_line(text, text_cap, &text_len, "result: PASS");
    }

    st_append(json, json_cap, &json_len, "{\"type\":\"boot_selftest\"");
    st_json_bool(json, json_cap, &json_len, "console_ready", r->console_ready, true);
    st_json_bool(json, json_cap, &json_len, "audit_ready", r->audit_ready, true);
    st_json_bool(json, json_cap, &json_len, "storage_ready", r->storage_ready, true);
    st_json_bool(json, json_cap, &json_len, "journal_active", r->journal_active, true);
    st_json_bool(json, json_cap, &json_len, "hw_snapshot_ready", r->hw_ready, true);
    st_json_bool(json, json_cap, &json_len, "proposal_engine_ready", r->proposal_ready, true);
    st_json_bool(json, json_cap, &json_len, "node_core_ready", r->node_ready, true);
    st_json_u32(json, json_cap, &json_len, "pass_count", r->pass_count, true);
    st_json_u32(json, json_cap, &json_len, "warn_count", r->warn_count, true);
    st_json_u32(json, json_cap, &json_len, "fail_count", r->fail_count, true);
    st_append(json, json_cap, &json_len, "}\n");
}

bool vita_selftest_run(const vita_command_context_t *ctx) {
    static char text[SELFTEST_TEXT_MAX];
    static char json[SELFTEST_JSON_MAX];
    vita_selftest_result_t r;
    bool ok_text = false;
    bool ok_json = false;

    st_build_result(ctx, &r);
    st_build_reports(&r, text, sizeof(text), json, sizeof(json));

    console_write_line("Boot self-test / Autoprueba de arranque:");
    st_print_line("console_ready", r.console_ready, false);
    st_print_line("storage_ready", r.storage_ready, false);
    st_print_line("audit_ready", r.audit_ready, true);
    st_print_line("journal_active", r.journal_active, true);
    st_print_line("hw_snapshot_ready", r.hw_ready, true);
    st_print_line("proposal_engine_ready", r.proposal_ready, true);
    st_print_line("node_core_ready", r.node_ready, true);

    if (r.storage_ready) {
        ok_text = storage_write_text(SELFTEST_TEXT_PATH, text);
        ok_json = storage_write_text(SELFTEST_JSONL_PATH, json);
        r.report_written = ok_text && ok_json;
    }

    console_write_line("report_written:");
    console_write_line(st_yes_no(r.report_written));
    if (r.report_written) {
        console_write_line(SELFTEST_TEXT_PATH);
        console_write_line(SELFTEST_JSONL_PATH);
    } else {
        console_write_line("self-test report not written; check storage readiness");
    }

    if (r.fail_count > 0U) {
        console_write_line("self-test result: FAIL");
    } else if (r.warn_count > 0U) {
        console_write_line("self-test result: WARN / restricted or partial mode");
    } else {
        console_write_line("self-test result: PASS");
    }

    audit_emit_boot_event("SELFTEST_RUN", r.report_written ? "self-test report written" : "self-test report not written");

    return r.fail_count == 0U;
}
CADD

# Add selftest module to COMMON_KERNEL.
if ! grep -q 'kernel/selftest.c' "$MAKEFILE"; then
  perl -0pi -e 's/(\tkernel\/node_core\.c)/$1 \\\n\tkernel\/selftest.c/' "$MAKEFILE"
fi

# Include selftest API in command core.
if ! grep -q '#include <vita/selftest.h>' "$COMMAND_C"; then
  perl -0pi -e 's/#include <vita\/proposal.h>\n/#include <vita\/proposal.h>\n#include <vita\/selftest.h>\n/' "$COMMAND_C"
fi

# Add command dispatch before shutdown.
if ! grep -q 'vita_selftest_run(ctx)' "$COMMAND_C"; then
  perl -0pi -e 's/(    if \(str_eq\(cmd, "shutdown"\) \|\| str_eq\(cmd, "exit"\)\) \{)/    if (str_eq(cmd, "selftest") ||\n        str_eq(cmd, "self-test") ||\n        str_eq(cmd, "boot selftest") ||\n        str_eq(cmd, "boot self-test") ||\n        str_eq(cmd, "checkup")) {\n        audit_emit_boot_event("COMMAND_SELFTEST", "selftest");\n        (void)vita_selftest_run(ctx);\n        return VITA_COMMAND_CONTINUE;\n    }\n\n$1/' "$COMMAND_C"
fi

cat > "$DOC" <<'DOC'
# Boot self-test command

Patch: `boot: add self-test command`

## Purpose

The self-test command gives the operator a compact health report for the current VitaOS F1A/F1B boot session.

It checks and reports:

- console readiness;
- audit readiness;
- storage readiness;
- journal activity;
- hardware snapshot availability;
- proposal engine readiness;
- node core readiness;
- current mode: operational or restricted diagnostic.

## Commands

```text
selftest
self-test
boot selftest
boot self-test
checkup
```

## Output files

When writable storage is available, VitaOS writes:

```text
/vita/export/reports/self-test.txt
/vita/export/reports/self-test.jsonl
```

## Scope boundaries

This command is a local check/report. It does not repair audit data, scan the whole filesystem, upload data, or add a GUI/window manager.

UEFI can still report restricted diagnostic mode until real persistent SQLite audit exists in the freestanding path.
DOC

# Basic sanity checks.
for sym in \
  'vita_selftest_run' \
  'SELFTEST_TEXT_PATH' \
  'SELFTEST_JSONL_PATH' \
  'boot self-test' \
  'kernel/selftest.c'; do
  if ! grep -Rqs "$sym" "$SELFTEST_H" "$SELFTEST_C" "$COMMAND_C" "$MAKEFILE" "$DOC" 2>/dev/null; then
    echo "[v31] ERROR: missing expected symbol: $sym" >&2
    exit 1
  fi
done

echo "[v31] boot self-test command patch applied"
