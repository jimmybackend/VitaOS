/*
 * kernel/session_export.c
 * Minimal F1A/F1B session summary export.
 */

#include <stdint.h>

#include <vita/audit.h>
#include <vita/console.h>
#include <vita/session_export.h>
#include <vita/storage.h>
#include <vita/session_transcript.h>

#define SESSION_EXPORT_PATH "/vita/export/reports/last-session.txt"
#define SESSION_EXPORT_BUFFER_MAX 4096U

static char g_session_export_buffer[SESSION_EXPORT_BUFFER_MAX];
static char g_session_export_readback[VITA_STORAGE_READ_MAX];

typedef struct {
    char *buf;
    unsigned long cap;
    unsigned long len;
} report_builder_t;

static const char *safe_text(const char *text, const char *fallback) {
    if (text && text[0]) {
        return text;
    }
    return fallback;
}

static const char *yes_no(bool value) {
    return value ? "yes" : "no";
}

static const char *boot_mode_name(const vita_handoff_t *handoff) {
    if (!handoff) {
        return "unknown";
    }

    if (handoff->firmware_type == VITA_FIRMWARE_HOSTED) {
        return "hosted";
    }

    if (handoff->firmware_type == VITA_FIRMWARE_UEFI) {
        return "uefi";
    }

    return "unknown";
}

static void rb_init(report_builder_t *rb, char *buf, unsigned long cap) {
    if (!rb) {
        return;
    }

    rb->buf = buf;
    rb->cap = cap;
    rb->len = 0;

    if (buf && cap > 0) {
        buf[0] = '\0';
    }
}

static void rb_append(report_builder_t *rb, const char *text) {
    unsigned long i;

    if (!rb || !rb->buf || rb->cap == 0 || !text) {
        return;
    }

    for (i = 0; text[i] && (rb->len + 1) < rb->cap; ++i) {
        rb->buf[rb->len++] = text[i];
    }

    rb->buf[rb->len] = '\0';
}

static void rb_line(report_builder_t *rb, const char *text) {
    rb_append(rb, text);
    rb_append(rb, "\n");
}

static void rb_kv(report_builder_t *rb, const char *key, const char *value) {
    rb_append(rb, key);
    rb_append(rb, value);
    rb_append(rb, "\n");
}

static void u64_to_dec(uint64_t value, char out[32]) {
    char tmp[32];
    int i = 0;
    int j = 0;

    if (!out) {
        return;
    }

    if (value == 0) {
        out[0] = '0';
        out[1] = '\0';
        return;
    }

    while (value > 0 && i < (int)(sizeof(tmp) - 1)) {
        tmp[i++] = (char)('0' + (value % 10ULL));
        value /= 10ULL;
    }

    while (i > 0) {
        out[j++] = tmp[--i];
    }

    out[j] = '\0';
}

static void rb_kv_u64(report_builder_t *rb, const char *key, uint64_t value) {
    char num[32];

    u64_to_dec(value, num);
    rb_kv(rb, key, num);
}

static void rb_kv_i32(report_builder_t *rb, const char *key, int value) {
    rb_kv_u64(rb, key, (uint64_t)((value < 0) ? 0 : value));
}

static void rb_kv_bool(report_builder_t *rb, const char *key, bool value) {
    rb_kv(rb, key, yes_no(value));
}

static bool text_exact_match(const char *a, const char *b) {
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

static void append_storage_status(report_builder_t *rb) {
    vita_storage_status_t st;

    storage_get_status(&st);

    rb_line(rb, "");
    rb_line(rb, "[storage]");
    rb_kv_bool(rb, "initialized: ", st.initialized);
    rb_kv_bool(rb, "writable: ", st.writable);
    rb_kv_bool(rb, "bootstrap_attempted: ", st.bootstrap_attempted);
    rb_kv_bool(rb, "bootstrap_verified: ", st.bootstrap_verified);
    rb_kv(rb, "backend: ", safe_text(st.backend_name, "none"));
    rb_kv(rb, "root_hint: ", safe_text(st.root_hint, "/vita"));
    rb_kv(rb, "last_error: ", safe_text(st.last_error, "none"));
}

static void append_hardware(report_builder_t *rb, const vita_command_context_t *ctx) {
    const vita_hw_snapshot_t *hw;

    rb_line(rb, "");
    rb_line(rb, "[hardware]");

    if (!ctx || !ctx->hw_ready) {
        rb_line(rb, "available: no");
        return;
    }

    hw = &ctx->hw_snapshot;
    rb_line(rb, "available: yes");
    rb_kv(rb, "cpu_arch: ", safe_text(hw->cpu_arch, "unknown"));
    rb_kv(rb, "cpu_model: ", safe_text(hw->cpu_model, "unavailable"));
    rb_kv_u64(rb, "ram_bytes: ", hw->ram_bytes);
    rb_kv(rb, "firmware_type: ", safe_text(hw->firmware_type, "unknown"));
    rb_kv(rb, "console_type: ", safe_text(hw->console_type, "unknown"));
    rb_kv_i32(rb, "display_count: ", hw->display_count);
    rb_kv_i32(rb, "keyboard_count: ", hw->keyboard_count);
    rb_kv_i32(rb, "mouse_count: ", hw->mouse_count);
    rb_kv_i32(rb, "audio_count: ", hw->audio_count);
    rb_kv_i32(rb, "microphone_count: ", hw->microphone_count);
    rb_kv_i32(rb, "net_count: ", hw->net_count);
    rb_kv_i32(rb, "ethernet_count: ", hw->ethernet_count);
    rb_kv_i32(rb, "wifi_count: ", hw->wifi_count);
    rb_kv_i32(rb, "storage_count: ", hw->storage_count);
    rb_kv_i32(rb, "usb_count: ", hw->usb_count);
    rb_kv_i32(rb, "usb_controller_count: ", hw->usb_controller_count);
    rb_kv_u64(rb, "detected_at_unix: ", hw->detected_at_unix);
}

static void append_network_summary(report_builder_t *rb, const vita_command_context_t *ctx) {
    const vita_hw_snapshot_t *hw;

    rb_line(rb, "");
    rb_line(rb, "[network]");

    if (!ctx || !ctx->hw_ready) {
        rb_line(rb, "available: unknown");
        rb_line(rb, "note: hardware snapshot unavailable");
        return;
    }

    hw = &ctx->hw_snapshot;
    rb_kv_i32(rb, "net_count: ", hw->net_count);
    rb_kv_i32(rb, "ethernet_count: ", hw->ethernet_count);
    rb_kv_i32(rb, "wifi_count: ", hw->wifi_count);

    if (ctx->handoff && ctx->handoff->firmware_type == VITA_FIRMWARE_HOSTED) {
        rb_line(rb, "transport_stage: hosted TCP test path may be available");
    } else {
        rb_line(rb, "transport_stage: UEFI diagnostic/status only for now");
    }

    rb_line(rb, "next_step: DHCP/TCP/remote AI transport remains staged work");
}

static void append_ai_summary(report_builder_t *rb) {
    rb_line(rb, "");
    rb_line(rb, "[ai_gateway]");
    rb_line(rb, "stage: command stub / local fallback");
    rb_line(rb, "commands: ai status, ai connect, ai ask, ai disconnect");
    rb_line(rb, "remote_call: not implemented in UEFI yet");
    rb_line(rb, "audit_policy: future remote decisions must produce auditable proposals");
}

static void append_notes(report_builder_t *rb) {
    rb_line(rb, "");
    rb_line(rb, "[notes]");
    rb_line(rb, "notes_path: /vita/notes/");
    rb_line(rb, "messages_path: /vita/messages/");
    rb_line(rb, "emergency_reports_path: /vita/emergency/reports/");
    rb_line(rb, "manual_review: use notes list and cat /vita/notes/<file>");
    rb_kv(rb, "transcript_txt_path: ", session_transcript_txt_path());
    rb_kv(rb, "transcript_jsonl_path: ", session_transcript_jsonl_path());
    rb_kv(rb, "transcript_state: ", session_transcript_state());
}

bool session_export_write_report(const vita_command_context_t *ctx) {
    report_builder_t rb;
    vita_storage_status_t st;

    if (!ctx) {
        console_write_line("export session: missing command context");
        return false;
    }

    rb_init(&rb, g_session_export_buffer, sizeof(g_session_export_buffer));

    rb_line(&rb, "VitaOS session summary / Resumen de sesion VitaOS");
    rb_line(&rb, "version: f1a-f1b-session-export-v1");
    rb_line(&rb, "");

    rb_line(&rb, "[boot]");
    rb_kv(&rb, "arch: ", safe_text(ctx->boot_status.arch_name, "unknown"));
    rb_kv(&rb, "boot_mode: ", boot_mode_name(ctx->handoff));
    rb_kv_bool(&rb, "console_ready: ", ctx->boot_status.console_ready);
    rb_kv_bool(&rb, "audit_ready: ", ctx->boot_status.audit_ready);
    rb_kv_bool(&rb, "restricted_diagnostic: ", !ctx->boot_status.audit_ready);
    rb_kv_bool(&rb, "proposal_engine_ready: ", ctx->proposal_ready);
    rb_kv_bool(&rb, "node_core_ready: ", ctx->node_ready);
    rb_kv_u64(&rb, "peers_discovered: ", ctx->console_state.peer_count);
    rb_kv_u64(&rb, "pending_proposals: ", ctx->console_state.pending_proposal_count);

    append_hardware(&rb, ctx);
    append_storage_status(&rb);
    append_network_summary(&rb, ctx);
    append_ai_summary(&rb);
    append_notes(&rb);

    rb_line(&rb, "");
    rb_line(&rb, "[limitations]");
    rb_line(&rb, "- UEFI SQLite persistence is not complete yet.");
    rb_line(&rb, "- UEFI network transport is diagnostic/staged unless later patches enable it.");
    rb_line(&rb, "- This report is a plain text checkpoint for human review.");

    rb_line(&rb, "");
    rb_kv(&rb, "saved_to: ", SESSION_EXPORT_PATH);

    audit_emit_boot_event("SESSION_EXPORT_REQUESTED", SESSION_EXPORT_PATH);

    if (storage_write_text(SESSION_EXPORT_PATH, g_session_export_buffer) &&
        storage_read_text(SESSION_EXPORT_PATH, g_session_export_readback, sizeof(g_session_export_readback)) &&
        text_exact_match(g_session_export_buffer, g_session_export_readback)) {
        console_write_line("export session: written");
        console_write_line(SESSION_EXPORT_PATH);
        audit_emit_boot_event("SESSION_EXPORT_WRITTEN", SESSION_EXPORT_PATH);
        return true;
    }

    storage_get_status(&st);
    console_write_line("export session: failed");
    console_write_line(st.last_error[0] ? st.last_error : "write/read verify failed");
    audit_emit_boot_event("SESSION_EXPORT_FAILED", st.last_error[0] ? st.last_error : "write/read verify failed");
    return false;
}
