/*
 * kernel/session_jsonl_export.c
 * Minimal F1A/F1B machine-readable JSONL session export.
 *
 * Scope:
 * - Writes /vita/export/reports/last-session.jsonl
 * - Uses the existing storage facade only
 * - Keeps output small and freestanding-friendly
 * - Does not dump SQLite or claim full audit persistence in UEFI
 */

#include <stdint.h>

#include <vita/audit.h>
#include <vita/console.h>
#include <vita/session_jsonl_export.h>
#include <vita/storage.h>
#include <vita/session_transcript.h>

#define SESSION_JSONL_EXPORT_PATH "/vita/export/reports/last-session.jsonl"
#define SESSION_JSONL_EXPORT_BUFFER_MAX 4096U

typedef struct {
    char *buf;
    unsigned long cap;
    unsigned long len;
} jsonl_builder_t;

static char g_session_jsonl_buffer[SESSION_JSONL_EXPORT_BUFFER_MAX];
static char g_session_jsonl_readback[VITA_STORAGE_READ_MAX];

static const char *safe_text(const char *text, const char *fallback) {
    if (text && text[0]) {
        return text;
    }
    return fallback;
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

static void jb_init(jsonl_builder_t *jb, char *buf, unsigned long cap) {
    if (!jb) {
        return;
    }

    jb->buf = buf;
    jb->cap = cap;
    jb->len = 0;

    if (buf && cap > 0) {
        buf[0] = '\0';
    }
}

static void jb_putc(jsonl_builder_t *jb, char c) {
    if (!jb || !jb->buf || jb->cap == 0) {
        return;
    }

    if ((jb->len + 1) >= jb->cap) {
        return;
    }

    jb->buf[jb->len++] = c;
    jb->buf[jb->len] = '\0';
}

static void jb_append(jsonl_builder_t *jb, const char *text) {
    unsigned long i;

    if (!jb || !text) {
        return;
    }

    for (i = 0; text[i]; ++i) {
        jb_putc(jb, text[i]);
    }
}

static void jb_json_string(jsonl_builder_t *jb, const char *text) {
    unsigned long i;
    const char *s = text ? text : "";

    jb_putc(jb, '"');

    for (i = 0; s[i]; ++i) {
        char c = s[i];

        if (c == '"' || c == '\\') {
            jb_putc(jb, '\\');
            jb_putc(jb, c);
        } else if (c == '\n') {
            jb_append(jb, "\\n");
        } else if (c == '\r') {
            jb_append(jb, "\\r");
        } else if (c == '\t') {
            jb_append(jb, "\\t");
        } else if ((unsigned char)c < 32U) {
            jb_putc(jb, '?');
        } else {
            jb_putc(jb, c);
        }
    }

    jb_putc(jb, '"');
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

static void json_begin(jsonl_builder_t *jb, const char *type) {
    jb_putc(jb, '{');
    jb_json_string(jb, "type");
    jb_putc(jb, ':');
    jb_json_string(jb, type);
}

static void json_prop_name(jsonl_builder_t *jb, const char *name) {
    jb_putc(jb, ',');
    jb_json_string(jb, name);
    jb_putc(jb, ':');
}

static void json_prop_str(jsonl_builder_t *jb, const char *name, const char *value) {
    json_prop_name(jb, name);
    jb_json_string(jb, value ? value : "");
}

static void json_prop_bool(jsonl_builder_t *jb, const char *name, bool value) {
    json_prop_name(jb, name);
    jb_append(jb, value ? "true" : "false");
}

static void json_prop_u64(jsonl_builder_t *jb, const char *name, uint64_t value) {
    char num[32];

    u64_to_dec(value, num);
    json_prop_name(jb, name);
    jb_append(jb, num);
}

static void json_prop_i32(jsonl_builder_t *jb, const char *name, int value) {
    json_prop_u64(jb, name, (uint64_t)((value < 0) ? 0 : value));
}

static void json_end(jsonl_builder_t *jb) {
    jb_putc(jb, '}');
    jb_putc(jb, '\n');
}

static bool path_has_jsonl_suffix(const char *path) {
    unsigned long len = 0;
    if (!path) return false;
    while (path[len]) len++;
    if (len < 6U) return false;
    return path[len - 6] == '.' && path[len - 5] == 'j' && path[len - 4] == 's' &&
           path[len - 3] == 'o' && path[len - 2] == 'n' && path[len - 1] == 'l';
}

static void build_jsonl_part_path(const char *base_path, unsigned int part_number, char *out, unsigned long cap) {
    unsigned long i = 0;
    unsigned long stem_len = 0;
    if (!out || cap == 0 || !base_path || !path_has_jsonl_suffix(base_path)) {
        if (out && cap > 0) out[0] = '\0';
        return;
    }
    while (base_path[stem_len]) stem_len++;
    stem_len -= 6U;
    while (i < stem_len && i + 1 < cap) {
        out[i] = base_path[i];
        i++;
    }
    if (part_number <= 1U) {
        out[i++] = '.';
        out[i++] = 'j';
        out[i++] = 's';
        out[i++] = 'o';
        out[i++] = 'n';
        out[i++] = 'l';
        out[i] = '\0';
        return;
    }
    if (i + 17U >= cap) {
        out[0] = '\0';
        return;
    }
    out[i++] = '.';
    out[i++] = 'p';
    out[i++] = 'a';
    out[i++] = 'r';
    out[i++] = 't';
    out[i++] = '-';
    out[i++] = (char)('0' + ((part_number / 1000U) % 10U));
    out[i++] = (char)('0' + ((part_number / 100U) % 10U));
    out[i++] = (char)('0' + ((part_number / 10U) % 10U));
    out[i++] = (char)('0' + (part_number % 10U));
    out[i++] = '.';
    out[i++] = 'j';
    out[i++] = 's';
    out[i++] = 'o';
    out[i++] = 'n';
    out[i++] = 'l';
    out[i] = '\0';
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

static void append_session_line(jsonl_builder_t *jb, const vita_command_context_t *ctx) {
    char boot_id[64];
    char node_id[64];
    vita_storage_status_t st;
    storage_get_status(&st);
    json_begin(jb, "session");
    json_prop_str(jb, "version", "f1a-f1b-session-jsonl-v1");
    json_prop_str(jb, "arch", safe_text(ctx->boot_status.arch_name, "unknown"));
    json_prop_str(jb, "boot_mode", boot_mode_name(ctx->handoff));
    if (audit_get_identity(boot_id, sizeof(boot_id), node_id, sizeof(node_id))) {
        json_prop_str(jb, "boot_id", safe_text(boot_id, "unknown"));
        json_prop_str(jb, "node_id", safe_text(node_id, "unknown"));
    } else {
        json_prop_str(jb, "boot_id", "unknown");
        json_prop_str(jb, "node_id", "unknown");
    }
    json_prop_str(jb, "host_id", "unknown");
    json_prop_str(jb, "firmware", (ctx->handoff && ctx->handoff->firmware_type == VITA_FIRMWARE_HOSTED) ? "hosted" : ((ctx->handoff && ctx->handoff->firmware_type == VITA_FIRMWARE_UEFI) ? "uefi" : "unknown"));
    json_prop_str(jb, "storage_backend", safe_text(st.backend_name, "unknown"));
    json_prop_str(jb, "storage_state", st.bootstrap_verified ? "verified" : "degraded");
    json_prop_bool(jb, "console_ready", ctx->boot_status.console_ready);
    json_prop_bool(jb, "audit_ready", ctx->boot_status.audit_ready);
    json_prop_bool(jb, "restricted_diagnostic", !ctx->boot_status.audit_ready);
    json_prop_bool(jb, "proposal_engine_ready", ctx->proposal_ready);
    json_prop_bool(jb, "node_core_ready", ctx->node_ready);
    json_prop_u64(jb, "peers_discovered", ctx->console_state.peer_count);
    json_prop_u64(jb, "pending_proposals", ctx->console_state.pending_proposal_count);
    json_end(jb);
}

static void append_hardware_line(jsonl_builder_t *jb, const vita_command_context_t *ctx) {
    const vita_hw_snapshot_t *hw;

    json_begin(jb, "hardware");
    json_prop_bool(jb, "available", ctx->hw_ready);

    if (ctx->hw_ready) {
        hw = &ctx->hw_snapshot;
        json_prop_str(jb, "cpu_arch", safe_text(hw->cpu_arch, "unknown"));
        json_prop_str(jb, "cpu_model", safe_text(hw->cpu_model, "unavailable"));
        json_prop_u64(jb, "ram_bytes", hw->ram_bytes);
        json_prop_str(jb, "firmware_type", safe_text(hw->firmware_type, "unknown"));
        json_prop_str(jb, "console_type", safe_text(hw->console_type, "unknown"));
        json_prop_i32(jb, "display_count", hw->display_count);
        json_prop_i32(jb, "keyboard_count", hw->keyboard_count);
        json_prop_i32(jb, "mouse_count", hw->mouse_count);
        json_prop_i32(jb, "audio_count", hw->audio_count);
        json_prop_i32(jb, "microphone_count", hw->microphone_count);
        json_prop_i32(jb, "net_count", hw->net_count);
        json_prop_i32(jb, "ethernet_count", hw->ethernet_count);
        json_prop_i32(jb, "wifi_count", hw->wifi_count);
        json_prop_i32(jb, "storage_count", hw->storage_count);
        json_prop_i32(jb, "usb_count", hw->usb_count);
        json_prop_i32(jb, "usb_controller_count", hw->usb_controller_count);
        json_prop_u64(jb, "detected_at_unix", hw->detected_at_unix);
    }

    json_end(jb);
}

static void append_storage_line(jsonl_builder_t *jb) {
    vita_storage_status_t st;

    storage_get_status(&st);

    json_begin(jb, "storage");
    json_prop_bool(jb, "initialized", st.initialized);
    json_prop_bool(jb, "writable", st.writable);
    json_prop_bool(jb, "bootstrap_attempted", st.bootstrap_attempted);
    json_prop_bool(jb, "bootstrap_verified", st.bootstrap_verified);
    json_prop_str(jb, "backend", safe_text(st.backend_name, "none"));
    json_prop_str(jb, "root_hint", safe_text(st.root_hint, "/vita"));
    json_prop_str(jb, "last_error", safe_text(st.last_error, "none"));
    json_end(jb);
}

static void append_network_line(jsonl_builder_t *jb, const vita_command_context_t *ctx) {
    const vita_hw_snapshot_t *hw = ctx->hw_ready ? &ctx->hw_snapshot : 0;

    json_begin(jb, "network");
    json_prop_bool(jb, "hardware_snapshot_available", ctx->hw_ready);
    json_prop_i32(jb, "net_count", hw ? hw->net_count : 0);
    json_prop_i32(jb, "ethernet_count", hw ? hw->ethernet_count : 0);
    json_prop_i32(jb, "wifi_count", hw ? hw->wifi_count : 0);
    json_prop_str(jb, "transport_stage",
                  (ctx->handoff && ctx->handoff->firmware_type == VITA_FIRMWARE_HOSTED)
                      ? "hosted_tcp_test_path"
                      : "uefi_diagnostic_status_only");
    json_prop_bool(jb, "dhcp_ready", false);
    json_prop_bool(jb, "wifi_credentials_ready", false);
    json_end(jb);
}

static void append_ai_line(jsonl_builder_t *jb) {
    json_begin(jb, "ai_gateway");
    json_prop_str(jb, "stage", "command_stub_local_fallback");
    json_prop_bool(jb, "remote_call_ready", false);
    json_prop_str(jb, "commands", "ai status, ai connect, ai ask, ai disconnect");
    json_prop_str(jb, "audit_policy", "future remote decisions must produce auditable proposals");
    json_end(jb);
}

static void append_paths_line(jsonl_builder_t *jb) {
    const char *base_jsonl = session_transcript_jsonl_path();
    unsigned int part_count = 0;
    unsigned int part = 2;
    char part_path[VITA_STORAGE_PATH_MAX];
    char readback[VITA_STORAGE_READ_MAX];

    json_begin(jb, "paths");
    json_prop_str(jb, "notes", "/vita/notes/");
    json_prop_str(jb, "messages", "/vita/messages/");
    json_prop_str(jb, "emergency_reports", "/vita/emergency/reports/");
    json_prop_str(jb, "exports", "/vita/export/");
    json_prop_str(jb, "jsonl_report", SESSION_JSONL_EXPORT_PATH);
    json_prop_str(jb, "transcript_txt_path", session_transcript_txt_path());
    json_prop_str(jb, "transcript_jsonl_path", base_jsonl);
    if (path_has_jsonl_suffix(base_jsonl)) {
        part_count = 1U;
        while (part <= 8U) {
            build_jsonl_part_path(base_jsonl, part, part_path, sizeof(part_path));
            if (!part_path[0] || !storage_read_text(part_path, readback, sizeof(readback))) {
                break;
            }
            part_count = part;
            part++;
        }
    }
    if (part_count > 1U) {
        json_prop_u64(jb, "transcript_jsonl_part_count", part_count);
        json_prop_name(jb, "transcript_jsonl_parts");
        jb_putc(jb, '[');
        part = 1;
        while (part <= part_count) {
            build_jsonl_part_path(base_jsonl, part, part_path, sizeof(part_path));
            if (part > 1U) {
                jb_putc(jb, ',');
            }
            jb_json_string(jb, part_path);
            part++;
        }
        jb_putc(jb, ']');
    }
    json_prop_str(jb, "transcript_state", session_transcript_state());
    json_end(jb);
}

static void append_limitations_line(jsonl_builder_t *jb) {
    json_begin(jb, "limitations");
    json_prop_bool(jb, "uefi_sqlite_persistence_complete", false);
    json_prop_bool(jb, "uefi_network_transport_complete", false);
    json_prop_bool(jb, "wifi_association_complete", false);
    json_prop_bool(jb, "full_vfs_complete", false);
    json_prop_str(jb, "note", "This JSONL report is a small machine-readable checkpoint, not a full SQLite audit export.");
    json_end(jb);
}

bool session_export_write_jsonl(const vita_command_context_t *ctx) {
    jsonl_builder_t jb;
    vita_storage_status_t st;

    if (!ctx) {
        console_write_line("export jsonl: missing command context");
        return false;
    }

    jb_init(&jb, g_session_jsonl_buffer, sizeof(g_session_jsonl_buffer));

    append_session_line(&jb, ctx);
    append_hardware_line(&jb, ctx);
    append_storage_line(&jb);
    append_network_line(&jb, ctx);
    append_ai_line(&jb);
    append_paths_line(&jb);
    append_limitations_line(&jb);

    audit_emit_boot_event("SESSION_JSONL_EXPORT_REQUESTED", SESSION_JSONL_EXPORT_PATH);

    if (storage_write_text(SESSION_JSONL_EXPORT_PATH, g_session_jsonl_buffer) &&
        storage_read_text(SESSION_JSONL_EXPORT_PATH, g_session_jsonl_readback, sizeof(g_session_jsonl_readback)) &&
        text_exact_match(g_session_jsonl_buffer, g_session_jsonl_readback)) {
        console_write_line("export jsonl: written");
        console_write_line(SESSION_JSONL_EXPORT_PATH);
        audit_emit_boot_event("SESSION_JSONL_EXPORT_WRITTEN", SESSION_JSONL_EXPORT_PATH);
        return true;
    }

    storage_get_status(&st);
    console_write_line("export jsonl: failed");
    console_write_line(st.last_error[0] ? st.last_error : "write/read verify failed");
    audit_emit_boot_event("SESSION_JSONL_EXPORT_FAILED", st.last_error[0] ? st.last_error : "write/read verify failed");
    return false;
}
