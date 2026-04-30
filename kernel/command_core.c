/*
 * kernel/command_core.c
 * Shared guided command loop for hosted and UEFI/F1A-F1B.
 */

#include <vita/audit.h>
#include <vita/ai_gateway.h>
#include <vita/command.h>
#include <vita/editor.h>
#include <vita/node.h>
#include <vita/session_export.h>
#include <vita/session_journal.h>
#include <vita/session_jsonl_export.h>
#include <vita/storage.h>
#include <vita/session_transcript.h>
#include <vita/proposal.h>
#include <vita/console_autocomplete.h>
#include <vita/vitair.h>
#include <vita/vitair_audit.h>

#ifdef VITA_HOSTED
#include <sqlite3.h>
#endif

static void mem_zero(void *ptr, unsigned long n) {
    unsigned char *p = (unsigned char *)ptr;
    unsigned long i;

    if (!ptr) {
        return;
    }

    for (i = 0; i < n; ++i) {
        p[i] = 0;
    }
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

static bool starts_with(const char *s, const char *prefix) {
    unsigned long i = 0;

    if (!s || !prefix) {
        return false;
    }

    while (prefix[i]) {
        if (s[i] != prefix[i]) {
            return false;
        }
        i++;
    }

    return true;
}

static bool is_space_char(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static const char *trim_left(const char *s) {
    if (!s) {
        return "";
    }

    while (*s && is_space_char(*s)) {
        s++;
    }

    return s;
}

static void trim_right_in_place(char *s) {
    unsigned long n = 0;

    if (!s) {
        return;
    }

    while (s[n]) {
        n++;
    }

    while (n > 0 && is_space_char(s[n - 1])) {
        s[--n] = '\0';
    }
}

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

static void u64_to_dec(uint64_t v, char out[32]) {
    char tmp[32];
    int i = 0;
    int j = 0;

    if (!out) {
        return;
    }

    if (v == 0) {
        out[0] = '0';
        out[1] = '\0';
        return;
    }

    while (v > 0 && i < (int)(sizeof(tmp) - 1)) {
        tmp[i++] = (char)('0' + (v % 10ULL));
        v /= 10ULL;
    }

    while (i > 0) {
        out[j++] = tmp[--i];
    }

    out[j] = '\0';
}

static void append_text(char *dst, size_t cap, size_t *len, const char *src) {
    size_t i = 0;

    if (!dst || !len || !src || cap == 0U) {
        return;
    }

    while (src[i] && *len + 1U < cap) {
        dst[*len] = src[i];
        *len = *len + 1U;
        i++;
    }

    dst[*len] = '\0';
}

static void console_write_i32(const char *label, int value) {
    char num[32];

    console_write_line(label);
    u64_to_dec((uint64_t)((value < 0) ? 0 : value), num);
    console_write_line(num);
}

static void resolve_audit_runtime_status(const vita_command_context_t *ctx, vita_audit_runtime_status_t *out) {
    vita_storage_status_t st;
    bool journal_active = session_journal_is_active();
    storage_get_status(&st);
    mem_zero(out, sizeof(*out));
    out->persistent_storage_writable = st.bootstrap_verified && st.writable;
    out->restricted_mode = !(ctx && ctx->boot_status.audit_ready);
    out->storage_status = st.bootstrap_verified ? VITA_STATUS_OK : (st.bootstrap_attempted ? VITA_STATUS_WARN : VITA_STATUS_FAIL);
    out->journal_txt_status = journal_active ? VITA_STATUS_OK : VITA_STATUS_WARN;
    out->journal_jsonl_status = journal_active ? VITA_STATUS_OK : VITA_STATUS_WARN;
    out->transcript_txt_status = journal_active ? VITA_STATUS_OK : VITA_STATUS_WARN;
    out->transcript_jsonl_status = journal_active ? VITA_STATUS_OK : VITA_STATUS_WARN;
    out->sqlite_status = (ctx && ctx->boot_status.audit_ready) ? VITA_STATUS_OK : VITA_STATUS_UNAVAILABLE;
}

static void command_refresh_state(vita_command_context_t *ctx) {
    if (!ctx) {
        return;
    }

    ctx->console_state.peer_count = (uint32_t)((ctx->node_ready) ? node_core_peer_count() : 0);
    ctx->console_state.pending_proposal_count = (uint32_t)((ctx->node_ready) ? node_core_pending_proposal_count() : 0);
    ctx->console_state.audit_ready = ctx->boot_status.audit_ready;
    ctx->console_state.proposal_engine_ready = ctx->proposal_ready;
    ctx->console_state.node_core_ready = ctx->node_ready;
}

void command_context_init(vita_command_context_t *ctx,
                          const vita_handoff_t *handoff,
                          const vita_boot_status_t *boot_status,
                          const vita_hw_snapshot_t *hw_snapshot,
                          bool hw_ready,
                          bool proposal_ready,
                          bool node_ready) {
    if (!ctx) {
        return;
    }

    mem_zero(ctx, sizeof(*ctx));

    ctx->handoff = handoff;
    ctx->hw_ready = hw_ready;
    ctx->proposal_ready = proposal_ready;
    ctx->node_ready = node_ready;

    if (boot_status) {
        ctx->boot_status.arch_name = boot_status->arch_name;
        ctx->boot_status.console_ready = boot_status->console_ready;
        ctx->boot_status.audit_ready = boot_status->audit_ready;
    } else {
        ctx->boot_status.arch_name = "unknown";
        ctx->boot_status.console_ready = false;
        ctx->boot_status.audit_ready = false;
    }

    if (hw_snapshot) {
        unsigned long i;

        for (i = 0; i < sizeof(ctx->hw_snapshot.cpu_arch); ++i) {
            ctx->hw_snapshot.cpu_arch[i] = hw_snapshot->cpu_arch[i];
        }

        for (i = 0; i < sizeof(ctx->hw_snapshot.cpu_model); ++i) {
            ctx->hw_snapshot.cpu_model[i] = hw_snapshot->cpu_model[i];
        }

        ctx->hw_snapshot.ram_bytes = hw_snapshot->ram_bytes;

        for (i = 0; i < sizeof(ctx->hw_snapshot.firmware_type); ++i) {
            ctx->hw_snapshot.firmware_type[i] = hw_snapshot->firmware_type[i];
        }

        for (i = 0; i < sizeof(ctx->hw_snapshot.console_type); ++i) {
            ctx->hw_snapshot.console_type[i] = hw_snapshot->console_type[i];
        }

        ctx->hw_snapshot.display_count = hw_snapshot->display_count;
        ctx->hw_snapshot.keyboard_count = hw_snapshot->keyboard_count;
        ctx->hw_snapshot.mouse_count = hw_snapshot->mouse_count;
        ctx->hw_snapshot.audio_count = hw_snapshot->audio_count;
        ctx->hw_snapshot.microphone_count = hw_snapshot->microphone_count;
        ctx->hw_snapshot.net_count = hw_snapshot->net_count;
        ctx->hw_snapshot.ethernet_count = hw_snapshot->ethernet_count;
        ctx->hw_snapshot.wifi_count = hw_snapshot->wifi_count;
        ctx->hw_snapshot.storage_count = hw_snapshot->storage_count;
        ctx->hw_snapshot.usb_count = hw_snapshot->usb_count;
        ctx->hw_snapshot.usb_controller_count = hw_snapshot->usb_controller_count;
        ctx->hw_snapshot.detected_at_unix = hw_snapshot->detected_at_unix;
    }

    ctx->console_state.arch_name = safe_text(ctx->boot_status.arch_name, "unknown");
    ctx->console_state.boot_mode = boot_mode_name(handoff);
    ctx->console_state.language_mode = "es,en";
    ctx->console_state.console_ready = ctx->boot_status.console_ready;
    ctx->console_state.audit_ready = ctx->boot_status.audit_ready;
    ctx->console_state.proposal_engine_ready = proposal_ready;
    ctx->console_state.node_core_ready = node_ready;
    ctx->console_state.peer_count = 0;
    ctx->console_state.pending_proposal_count = 0;
    ctx->console_state.mode = ctx->boot_status.audit_ready
        ? VITA_CONSOLE_MODE_GUIDED
        : VITA_CONSOLE_MODE_AUDIT;

    command_refresh_state(ctx);
}

static void show_hw(const vita_command_context_t *ctx) {
    const vita_hw_snapshot_t *hw;
    char num[32];

    if (!ctx || !ctx->hw_ready) {
        console_write_line("HW Snapshot / Resumen de hardware:");
        console_write_line("No hardware snapshot available / No hay resumen de hardware disponible");
        return;
    }

    hw = &ctx->hw_snapshot;

    console_write_line("HW Snapshot / Resumen de hardware:");

    console_write_line("cpu_arch:");
    console_write_line(hw->cpu_arch[0] ? hw->cpu_arch : "unknown");

    console_write_line("cpu_model:");
    console_write_line(hw->cpu_model[0] ? hw->cpu_model : "unavailable");

    console_write_line("ram_bytes:");
    u64_to_dec(hw->ram_bytes, num);
    console_write_line(num);

    console_write_line("firmware_type:");
    console_write_line(hw->firmware_type[0] ? hw->firmware_type : "unknown");

    console_write_line("console_type:");
    console_write_line(hw->console_type[0] ? hw->console_type : "unknown");

    console_write_i32("display_count:", hw->display_count);
    console_write_i32("keyboard_count:", hw->keyboard_count);
    console_write_i32("mouse_count:", hw->mouse_count);
    console_write_i32("audio_count:", hw->audio_count);
    console_write_i32("microphone_count:", hw->microphone_count);

    console_write_i32("net_count:", hw->net_count);
    console_write_i32("ethernet_count:", hw->ethernet_count);
    console_write_i32("wifi_count:", hw->wifi_count);

    console_write_i32("storage_count:", hw->storage_count);
    console_write_i32("usb_count:", hw->usb_count);
    console_write_i32("usb_controller_count:", hw->usb_controller_count);

    console_write_line("detected_at_unix:");
    u64_to_dec(hw->detected_at_unix, num);
    console_write_line(num);
}

static void show_audit(const vita_command_context_t *ctx) {
    vita_audit_runtime_status_t rt;
    vita_ir_claim_t claims[VITA_IR_AUDIT_CLAIM_MAX];
    size_t claim_count;
    size_t i;

    console_write_line("Audit / Auditoria:");
    resolve_audit_runtime_status(ctx, &rt);
    console_write_line(rt.journal_jsonl_status == VITA_STATUS_OK
        ? "Audit journal TXT/JSONL: OK"
        : "Audit journal TXT/JSONL: WARN");
    if (rt.sqlite_status == VITA_STATUS_OK) {
        console_write_line("Audit SQLite: READY / Auditoria SQLite: LISTA");
    } else {
        console_write_line("Audit SQLite: UNAVAILABLE / Auditoria SQLite: NO DISPONIBLE");
    }

    if (rt.sqlite_status == VITA_STATUS_OK) {
        console_write_line("Persistent audit backend is active for this session.");
        console_write_line("La bitacora persistente esta activa para esta sesion.");
        console_write_line(rt.journal_jsonl_status == VITA_STATUS_OK
            ? "audit persistence: journal/jsonl active; sqlite hosted ready"
            : "audit persistence: sqlite hosted ready; journal inactive");
    } else {
        console_write_line(rt.journal_jsonl_status == VITA_STATUS_OK
            ? "audit persistence: journal/jsonl active (restricted mode)"
            : "audit persistence: restricted diagnostic");
        console_write_line("Restricted diagnostic mode is active.");
        console_write_line("Modo diagnostico restringido activo.");
        console_write_line("storage_writable:");
        console_write_line(rt.persistent_storage_writable ? "true" : "false");
        console_write_line("UEFI physical boot can still run local console commands, but it must not claim full operational mode until persistent audit exists.");
        console_write_line("En UEFI fisico puedes usar comandos locales, pero el modo operativo completo requiere auditoria persistente.");
    }

    claim_count = vita_ir_claims_from_audit_runtime(&rt, claims, VITA_IR_AUDIT_CLAIM_MAX);
    if (claim_count == 0U) {
        console_write_line("VitaIR-Tri runtime claims: unavailable");
        return;
    }

    console_write_line("VitaIR-Tri runtime claims:");
    for (i = 0; i < claim_count; ++i) {
        char line[160];
        size_t pos = 0U;
        const char *claim = (claims[i].claim && claims[i].claim[0]) ? claims[i].claim : "unknown.claim";
        const char *symbol = vita_tri_to_symbol(claims[i].state);
        const char *severity = vita_ir_severity_to_string(claims[i].severity);

        if (!symbol || !symbol[0]) {
            symbol = "0";
        }
        if (!severity || !severity[0]) {
            severity = "warn";
        }

        line[0] = '\0';
        append_text(line, sizeof(line), &pos, "- ");
        append_text(line, sizeof(line), &pos, claim);
        append_text(line, sizeof(line), &pos, ": ");
        append_text(line, sizeof(line), &pos, symbol);
        append_text(line, sizeof(line), &pos, " ");
        append_text(line, sizeof(line), &pos, severity);
        console_write_line(line);
    }
}

static void show_status_vitair_summary(const vita_command_context_t *ctx) {
    static const char *k_summary_claims[] = {
        "storage.persistent.writable",
        "audit.journal_jsonl.available",
        "audit.sqlite.available",
        "operational.restricted",
        "operational.full"
    };
    vita_audit_runtime_status_t rt;
    vita_ir_claim_t claims[VITA_IR_AUDIT_CLAIM_MAX];
    size_t claim_count;
    size_t i;

    resolve_audit_runtime_status(ctx, &rt);
    claim_count = vita_ir_claims_from_audit_runtime(&rt, claims, VITA_IR_AUDIT_CLAIM_MAX);

    if (claim_count == 0U) {
        console_write_line("VitaIR-Tri status summary: unavailable");
        return;
    }

    console_write_line("VitaIR-Tri status summary:");
    for (i = 0; i < (sizeof(k_summary_claims) / sizeof(k_summary_claims[0])); ++i) {
        const char *target_claim = k_summary_claims[i];
        size_t j;

        for (j = 0; j < claim_count; ++j) {
            if (str_eq(claims[j].claim, target_claim)) {
                char line[160];
                size_t pos = 0U;
                const char *symbol = vita_tri_to_symbol(claims[j].state);
                const char *severity = vita_ir_severity_to_string(claims[j].severity);

                if (!symbol || !symbol[0]) {
                    symbol = "0";
                }
                if (!severity || !severity[0]) {
                    severity = "warn";
                }

                line[0] = '\0';
                append_text(line, sizeof(line), &pos, "- ");
                append_text(line, sizeof(line), &pos, target_claim);
                append_text(line, sizeof(line), &pos, ": ");
                append_text(line, sizeof(line), &pos, symbol);
                append_text(line, sizeof(line), &pos, " ");
                append_text(line, sizeof(line), &pos, severity);
                console_write_line(line);
                break;
            }
        }
    }
}

static void show_storage_status_vitair_summary(const vita_command_context_t *ctx) {
    static const char *k_storage_summary_claims[] = {
        "storage.persistent.writable",
        "audit.journal_jsonl.available",
        "operational.restricted",
        "operational.full"
    };
    vita_audit_runtime_status_t rt;
    vita_ir_claim_t claims[VITA_IR_AUDIT_CLAIM_MAX];
    size_t claim_count;
    size_t i;
    bool printed = false;

    resolve_audit_runtime_status(ctx, &rt);
    claim_count = vita_ir_claims_from_audit_runtime(&rt, claims, VITA_IR_AUDIT_CLAIM_MAX);
    if (claim_count == 0U) {
        console_write_line("VitaIR-Tri storage summary: unavailable");
        return;
    }

    console_write_line("VitaIR-Tri storage summary:");
    for (i = 0; i < (sizeof(k_storage_summary_claims) / sizeof(k_storage_summary_claims[0])); ++i) {
        const char *target_claim = k_storage_summary_claims[i];
        size_t j;

        for (j = 0; j < claim_count; ++j) {
            if (str_eq(claims[j].claim, target_claim)) {
                char line[160];
                size_t pos = 0U;
                const char *symbol = vita_tri_to_symbol(claims[j].state);
                const char *severity = vita_ir_severity_to_string(claims[j].severity);

                if (!symbol || !symbol[0]) {
                    symbol = "0";
                }
                if (!severity || !severity[0]) {
                    severity = "warn";
                }

                line[0] = '\0';
                append_text(line, sizeof(line), &pos, "- ");
                append_text(line, sizeof(line), &pos, target_claim);
                append_text(line, sizeof(line), &pos, ": ");
                append_text(line, sizeof(line), &pos, symbol);
                append_text(line, sizeof(line), &pos, " ");
                append_text(line, sizeof(line), &pos, severity);
                console_write_line(line);
                printed = true;
                break;
            }
        }
    }

    if (!printed) {
        console_write_line("VitaIR-Tri storage summary: unavailable");
    }
}

static void show_peers(const vita_command_context_t *ctx) {
    if (ctx && ctx->node_ready) {
        node_core_show_peers();
        return;
    }

    console_write_line("Peers / Nodos cooperativos:");
    console_write_line("No peer transport active in this boot mode.");
    console_write_line("No hay transporte de peers activo en este modo de arranque.");
}

static void show_proposals(const vita_command_context_t *ctx) {
    if (!ctx || !ctx->proposal_ready) {
        console_write_line("No proposals available / No hay propuestas disponibles");
        return;
    }

    proposal_show_all();

    if (ctx->node_ready && node_core_pending_proposal_count() > 0) {
        node_core_show_link_proposal();
    }
}

static void show_emergency(const vita_command_context_t *ctx, const char *free_text) {
    console_write_line("Emergency guided flow / Flujo de emergencia guiada:");
    console_write_line("");

    console_write_line("1. Resumen entendido / Understood summary:");
    if (free_text && free_text[0]) {
        console_write_line(free_text);
    } else {
        console_write_line("Emergency mode requested / Modo emergencia solicitado");
    }

    console_write_line("2. Riesgos inmediatos / Immediate risks:");
    console_write_line("Protect life first: breathing, bleeding, fire, unsafe structure, cold/heat, dehydration, getting lost.");
    console_write_line("Primero protege la vida: respiracion, sangrado, fuego, estructura insegura, frio/calor, deshidratacion, extravio.");

    console_write_line("3. Preguntas criticas / Critical questions:");
    console_write_line("How many people are affected? Is anyone unconscious, bleeding, trapped, or in immediate danger?");
    console_write_line("Cuantas personas estan afectadas? Alguien esta inconsciente, sangrando, atrapado o en peligro inmediato?");
    console_write_line("Where are you, what resources do you have, and can you safely stay in place?");
    console_write_line("Donde estas, que recursos tienes y puedes quedarte en un lugar seguro?");

    console_write_line("4. Acciones inmediatas sugeridas / Suggested immediate actions:");
    console_write_line("Stop, secure the area, count people, treat life-threatening bleeding, preserve heat, conserve water, signal for help.");
    console_write_line("Detente, asegura el area, cuenta personas, atiende sangrado grave, conserva calor, ahorra agua, senaliza ayuda.");

    console_write_line("5. Recursos del sistema disponibles / Available system resources:");
    if (ctx && ctx->hw_ready) {
        show_hw(ctx);
    } else {
        console_write_line("Hardware summary unavailable / Resumen de hardware no disponible");
    }

    console_write_line("6. Propuesta de accion del sistema / System proposal:");
    console_write_line("Use 'proposals' to review available actions, then 'approve <id>' or 'reject <id>'.");
    console_write_line("Usa 'proposals' para revisar acciones disponibles, luego 'approve <id>' o 'reject <id>'.");

    console_write_line("7. Estado de auditoria / Audit state:");
    if (ctx && ctx->boot_status.audit_ready) {
        console_write_line("READY / LISTA");
    } else {
        console_write_line("FAILED - restricted diagnostic mode / FALLA - modo diagnostico restringido");
    }

    audit_emit_boot_event("EMERGENCY_FLOW_SHOWN", free_text && free_text[0] ? free_text : "emergency command");
}

static void show_refreshed_header(vita_command_context_t *ctx) {
    if (!ctx) {
        return;
    }

    command_refresh_state(ctx);
    console_guided_show_welcome(&ctx->console_state);
    console_guided_show_menu();
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

static bool write_report_verified(const char *path, const char *body) {
    static char readback[VITA_STORAGE_READ_MAX];
    const char *expected = body ? body : "";

    if (!storage_write_text(path, expected)) {
        console_write_line("report write failed:");
        console_write_line(path ? path : "(null)");
        console_write_line(storage_last_error());
        return false;
    }

    if (!storage_read_text(path, readback, sizeof(readback))) {
        console_write_line("report verify read failed:");
        console_write_line(path ? path : "(null)");
        console_write_line(storage_last_error());
        return false;
    }

    if (!text_exact_match(readback, expected)) {
        console_write_line("report verify compare failed:");
        console_write_line(path ? path : "(null)");
        console_write_line("write/read verify failed");
        return false;
    }

    return true;
}

static bool write_report_pair_verified(const char *txt_path, const char *txt_body,
                                       const char *jsonl_path, const char *jsonl_body) {
    if (!write_report_verified(txt_path, txt_body)) {
        return false;
    }

    if (!write_report_verified(jsonl_path, jsonl_body)) {
        return false;
    }

    return true;
}

static void handle_audit_readiness(const vita_command_context_t *ctx) {
    show_audit(ctx);
}

static void handle_audit_verify(const vita_command_context_t *ctx) {
    console_write_line("Audit verify / Verificacion de auditoria:");
    if (ctx && ctx->boot_status.audit_ready) {
        console_write_line("hosted hash chain: OK (current boot)");
        console_write_line("cadena hash hosted: OK (arranque actual)");
    } else {
        console_write_line("UEFI restricted diagnostic: hash verification unavailable");
        console_write_line("UEFI diagnostico restringido: verificacion hash no disponible");
    }
}

static void handle_audit_export(const vita_command_context_t *ctx) {
    const char *txt_path = "/vita/export/audit/audit-verify.txt";
    const char *jsonl_path = "/vita/export/audit/audit-verify.jsonl";
    const char *txt_ok =
        "audit_verify: ok\n"
        "scope: current boot hash chain\n";
    const char *txt_limited =
        "audit_verify: unavailable\n"
        "scope: uefi restricted diagnostic mode\n";
    const char *jsonl_ok = "{\"type\":\"audit_verify\",\"status\":\"ok\",\"scope\":\"current_boot\"}\n";
    const char *jsonl_limited = "{\"type\":\"audit_verify\",\"status\":\"unavailable\",\"scope\":\"uefi_restricted\"}\n";

    if (write_report_pair_verified(txt_path,
                          (ctx && ctx->boot_status.audit_ready) ? txt_ok : txt_limited,
                          jsonl_path,
                          (ctx && ctx->boot_status.audit_ready) ? jsonl_ok : jsonl_limited)) {
        console_write_line("audit export: written");
        console_write_line(txt_path);
        console_write_line(jsonl_path);
        return;
    }

    console_write_line("audit export: failed");
}

static void handle_audit_events_export(const vita_command_context_t *ctx) {
    const char *txt_path = "/vita/export/audit/current-session-events.txt";
    const char *jsonl_path = "/vita/export/audit/current-session-events.jsonl";
    const char *txt =
        "current_session_events: minimal export\n"
        "note: use hosted SQLite for full query depth\n";
    const char *jsonl = "{\"type\":\"current_session_events\",\"status\":\"exported\",\"mode\":\"minimal\"}\n";
    (void)ctx;

    if (write_report_pair_verified(txt_path, txt, jsonl_path, jsonl)) {
        console_write_line("audit events: written");
        console_write_line(txt_path);
        console_write_line(jsonl_path);
        return;
    }

    console_write_line("audit events: failed");
}

static void handle_audit_sqlite_summary(const vita_command_context_t *ctx) {
#ifdef VITA_HOSTED
    sqlite3 *db = 0;
    sqlite3_stmt *st = 0;
    const char *path = (ctx && ctx->handoff && ctx->handoff->audit_db_path && ctx->handoff->audit_db_path[0])
        ? ctx->handoff->audit_db_path
        : "build/audit/vitaos-audit.db";
    int rc;

    rc = sqlite3_open(path, &db);
    if (rc != SQLITE_OK || !db) {
        console_write_line("audit sqlite: open failed");
        if (db) {
            sqlite3_close(db);
        }
        return;
    }

    rc = sqlite3_prepare_v2(db,
                            "SELECT "
                            "(SELECT count(*) FROM boot_session),"
                            "(SELECT count(*) FROM audit_event),"
                            "(SELECT count(*) FROM hardware_snapshot),"
                            "(SELECT count(*) FROM ai_proposal),"
                            "(SELECT count(*) FROM node_peer)",
                            -1,
                            &st,
                            0);
    if (rc == SQLITE_OK && st && sqlite3_step(st) == SQLITE_ROW) {
        char num[32];
        console_write_line("audit sqlite summary:");
        u64_to_dec((uint64_t)sqlite3_column_int64(st, 0), num);
        console_write_line("boot_session:");
        console_write_line(num);
        u64_to_dec((uint64_t)sqlite3_column_int64(st, 1), num);
        console_write_line("audit_event:");
        console_write_line(num);
        u64_to_dec((uint64_t)sqlite3_column_int64(st, 2), num);
        console_write_line("hardware_snapshot:");
        console_write_line(num);
        u64_to_dec((uint64_t)sqlite3_column_int64(st, 3), num);
        console_write_line("ai_proposal:");
        console_write_line(num);
        u64_to_dec((uint64_t)sqlite3_column_int64(st, 4), num);
        console_write_line("node_peer:");
        console_write_line(num);
    } else {
        console_write_line("audit sqlite: query failed");
    }

    if (st) {
        sqlite3_finalize(st);
    }
    sqlite3_close(db);
#else
    (void)ctx;
    console_write_line("audit sqlite: unavailable in UEFI/freestanding mode");
#endif
}

static void handle_diagnostic_bundle(const vita_command_context_t *ctx) {
    vita_storage_status_t st;
    const char *txt_ready;
    const char *txt_limited;
    const char *jsonl_ready;
    const char *jsonl_limited;
    const char *txt_path = "/vita/export/reports/diagnostic-bundle.txt";
    const char *jsonl_path = "/vita/export/reports/diagnostic-bundle.jsonl";
    storage_get_status(&st);
    if (st.bootstrap_verified && starts_with(st.backend_name, "uefi_simple_fs")) {
        txt_ready = "Diagnostico VitaOS / VitaOS diagnostic\n[estado]\ndiagnostic_bundle: generated\n[auditoria]\naudit_mode: hosted_sqlite_ready\n[almacenamiento]\narch: x86_64\nfirmware: hosted\nboot_mode: hosted\nboot_id: unknown\nnode_id: unknown\nhost_id: unknown\nstorage_backend: uefi_simple_fs\nstorage_state: verified\n[hardware]\nhardware_snapshot: available\n[limitaciones]\nwifi_transport: no implementado\nnetwork_remote_ai: no implementado\n";
        txt_limited = "Diagnostico VitaOS / VitaOS diagnostic\n[estado]\ndiagnostic_bundle: generated\n[auditoria]\naudit_mode: uefi_restricted_diagnostic\n[almacenamiento]\narch: x86_64\nfirmware: uefi\nboot_mode: uefi\nboot_id: unknown\nnode_id: unknown\nhost_id: unknown\nstorage_backend: uefi_simple_fs\nstorage_state: verified\n[hardware]\nhardware_snapshot: no disponible\n[limitaciones]\nwifi_transport: no implementado\nnetwork_remote_ai: no implementado\n";
        jsonl_ready = "{\"type\":\"diagnostic_bundle\",\"audit_mode\":\"hosted_sqlite_ready\",\"arch\":\"x86_64\",\"firmware\":\"hosted\",\"boot_mode\":\"hosted\",\"boot_id\":\"unknown\",\"node_id\":\"unknown\",\"host_id\":\"unknown\",\"storage_backend\":\"uefi_simple_fs\",\"storage_state\":\"verified\"}\n";
        jsonl_limited = "{\"type\":\"diagnostic_bundle\",\"audit_mode\":\"uefi_restricted_diagnostic\",\"arch\":\"x86_64\",\"firmware\":\"uefi\",\"boot_mode\":\"uefi\",\"boot_id\":\"unknown\",\"node_id\":\"unknown\",\"host_id\":\"unknown\",\"storage_backend\":\"uefi_simple_fs\",\"storage_state\":\"verified\"}\n";
    } else {
        txt_ready = "Diagnostico VitaOS / VitaOS diagnostic\n[estado]\ndiagnostic_bundle: generated\n[auditoria]\naudit_mode: hosted_sqlite_ready\n[almacenamiento]\narch: x86_64\nfirmware: hosted\nboot_mode: hosted\nboot_id: unknown\nnode_id: unknown\nhost_id: unknown\nstorage_backend: hostfs\nstorage_state: verified\n[hardware]\nhardware_snapshot: available\n[limitaciones]\nwifi_transport: no implementado\nnetwork_remote_ai: no implementado\n";
        txt_limited = "Diagnostico VitaOS / VitaOS diagnostic\n[estado]\ndiagnostic_bundle: generated\n[auditoria]\naudit_mode: uefi_restricted_diagnostic\n[almacenamiento]\narch: x86_64\nfirmware: uefi\nboot_mode: uefi\nboot_id: unknown\nnode_id: unknown\nhost_id: unknown\nstorage_backend: unknown\nstorage_state: degraded\n[hardware]\nhardware_snapshot: no disponible\n[limitaciones]\nwifi_transport: no implementado\nnetwork_remote_ai: no implementado\n";
        jsonl_ready = "{\"type\":\"diagnostic_bundle\",\"audit_mode\":\"hosted_sqlite_ready\",\"arch\":\"x86_64\",\"firmware\":\"hosted\",\"boot_mode\":\"hosted\",\"boot_id\":\"unknown\",\"node_id\":\"unknown\",\"host_id\":\"unknown\",\"storage_backend\":\"hostfs\",\"storage_state\":\"verified\"}\n";
        jsonl_limited = "{\"type\":\"diagnostic_bundle\",\"audit_mode\":\"uefi_restricted_diagnostic\",\"arch\":\"x86_64\",\"firmware\":\"uefi\",\"boot_mode\":\"uefi\",\"boot_id\":\"unknown\",\"node_id\":\"unknown\",\"host_id\":\"unknown\",\"storage_backend\":\"unknown\",\"storage_state\":\"degraded\"}\n";
    }

    if (write_report_pair_verified(txt_path,
                          (ctx && ctx->boot_status.audit_ready) ? txt_ready : txt_limited,
                          jsonl_path,
                          (ctx && ctx->boot_status.audit_ready) ? jsonl_ready : jsonl_limited)) {
        console_write_line("diagnostic: written");
        return;
    }

    console_write_line("diagnostic: failed");
}

static void handle_export_index(void) {
    const char *txt_path = "/vita/export/export-index.txt";
    const char *jsonl_path = "/vita/export/export-index.jsonl";
    char txt[1024];
    char probe[8];
    unsigned long n = 0;
#define APPEND_LINE(path_literal) do { \
    if (storage_read_text(path_literal, probe, sizeof(probe))) { \
        const char *p = "- " path_literal "\n"; \
        unsigned long i = 0; \
        while (p[i] && (n + 1U) < sizeof(txt)) { txt[n++] = p[i++]; } \
    } \
} while (0)
    const char *header = "export_index:\n";
    while (header[n] && (n + 1U) < sizeof(txt)) { txt[n] = header[n]; n++; }
    APPEND_LINE("/vita/export/reports/last-session.txt");
    APPEND_LINE("/vita/export/reports/last-session.jsonl");
    APPEND_LINE("/vita/export/reports/diagnostic-bundle.txt");
    APPEND_LINE("/vita/export/reports/diagnostic-bundle.jsonl");
    APPEND_LINE("/vita/export/reports/self-test.txt");
    APPEND_LINE("/vita/export/reports/self-test.jsonl");
    APPEND_LINE("/vita/export/reports/vitair-state.jsonl");
    APPEND_LINE("/vita/export/audit/audit-verify.txt");
    APPEND_LINE("/vita/export/audit/audit-verify.jsonl");
    APPEND_LINE("/vita/export/audit/current-session-events.txt");
    APPEND_LINE("/vita/export/audit/current-session-events.jsonl");
    txt[n] = '\0';
#undef APPEND_LINE
    const char *jsonl = "{\"type\":\"export_index\",\"status\":\"written\"}\n";

    if (write_report_pair_verified(txt_path, txt, jsonl_path, jsonl)) {
        console_write_line("export index: written");
        return;
    }

    console_write_line("export index: failed");
}

static void handle_export_vitair(const vita_command_context_t *ctx) {
    vita_audit_runtime_status_t rt;
    vita_ir_claim_t claims[VITA_IR_AUDIT_CLAIM_MAX];
    size_t claim_count;
    size_t i;
    const char *jsonl_path = "/vita/export/reports/vitair-state.jsonl";
    static char jsonl_report[4096];
    size_t jsonl_len = 0U;

    resolve_audit_runtime_status(ctx, &rt);
    claim_count = vita_ir_claims_from_audit_runtime(&rt, claims, VITA_IR_AUDIT_CLAIM_MAX);

    if (claim_count == 0U) {
        append_text(jsonl_report, sizeof(jsonl_report), &jsonl_len,
                    "{\"type\":\"vitair_claims\",\"ir_version\":\"" VITA_IR_VERSION "\",\"status\":\"unavailable\"}\n");
    } else {
        for (i = 0; i < claim_count; ++i) {
            char jsonl_line[384];
            size_t jsonl_line_len = 0U;
            const char *claim = safe_text(claims[i].claim, "unknown.claim");
            const char *state_num = vita_tri_to_json_number(claims[i].state);
            const char *severity = vita_ir_severity_to_string(claims[i].severity);
            const char *actor = safe_text(claims[i].actor, "unknown");
            const char *target = safe_text(claims[i].target, "unknown");
            const char *meaning = safe_text(claims[i].meaning, "unknown");
            const char *effect = safe_text(claims[i].effect, "unknown");

            if (!state_num || !state_num[0]) {
                state_num = "0";
            }
            if (!severity || !severity[0]) {
                severity = "warn";
            }

            jsonl_line[0] = '\0';
            append_text(jsonl_line, sizeof(jsonl_line), &jsonl_line_len,
                        "{\"type\":\"vitair_claim\",\"ir_version\":\"");
            append_text(jsonl_line, sizeof(jsonl_line), &jsonl_line_len, VITA_IR_VERSION);
            append_text(jsonl_line, sizeof(jsonl_line), &jsonl_line_len,
                        "\",\"claim\":\"");
            append_text(jsonl_line, sizeof(jsonl_line), &jsonl_line_len, claim);
            append_text(jsonl_line, sizeof(jsonl_line), &jsonl_line_len,
                        "\",\"state\":");
            append_text(jsonl_line, sizeof(jsonl_line), &jsonl_line_len, state_num);
            append_text(jsonl_line, sizeof(jsonl_line), &jsonl_line_len,
                        ",\"severity\":\"");
            append_text(jsonl_line, sizeof(jsonl_line), &jsonl_line_len, severity);
            append_text(jsonl_line, sizeof(jsonl_line), &jsonl_line_len,
                        "\",\"actor\":\"");
            append_text(jsonl_line, sizeof(jsonl_line), &jsonl_line_len, actor);
            append_text(jsonl_line, sizeof(jsonl_line), &jsonl_line_len,
                        "\",\"target\":\"");
            append_text(jsonl_line, sizeof(jsonl_line), &jsonl_line_len, target);
            append_text(jsonl_line, sizeof(jsonl_line), &jsonl_line_len,
                        "\",\"meaning\":\"");
            append_text(jsonl_line, sizeof(jsonl_line), &jsonl_line_len, meaning);
            append_text(jsonl_line, sizeof(jsonl_line), &jsonl_line_len,
                        "\",\"effect\":\"");
            append_text(jsonl_line, sizeof(jsonl_line), &jsonl_line_len, effect);
            append_text(jsonl_line, sizeof(jsonl_line), &jsonl_line_len,
                        "\"}\n");

            append_text(jsonl_report, sizeof(jsonl_report), &jsonl_len, jsonl_line);
        }
    }

    if (write_report_verified(jsonl_path, jsonl_report)) {
        console_write_line("export vitair: written");
        console_write_line(jsonl_path);
        return;
    }

    console_write_line("export vitair: failed");
}

static void handle_selftest(const vita_command_context_t *ctx) {
    vita_audit_runtime_status_t rt;
    vita_ir_claim_t claims[VITA_IR_AUDIT_CLAIM_MAX];
    size_t claim_count;
    size_t i;
    const char *txt_path = "/vita/export/reports/self-test.txt";
    const char *jsonl_path = "/vita/export/reports/self-test.jsonl";
    const char *txt_pass =
        "Resultado: PASS/WARN/FAIL -> PASS\n"
        "Consola: PASS\n"
        "Almacenamiento: OK (verified writable)\n"
        "Audit journal TXT/JSONL: OK\n"
        "Audit SQLite: OK\n"
        "Operational mode: full\n"
        "Limitaciones: no implementado wifi/network remoto\n";
    const char *txt_warn_storage_ok =
        "Resultado: PASS/WARN/FAIL -> WARN\n"
        "Consola: PASS\n"
        "Almacenamiento: OK (verified writable)\n"
        "Audit journal TXT/JSONL: OK\n"
        "Audit SQLite: UNAVAILABLE (uefi restricted diagnostic)\n"
        "Operational mode: restricted diagnostic\n"
        "Limitaciones: no implementado wifi/network remoto\n";
    const char *txt_warn_storage_degraded =
        "Resultado: PASS/WARN/FAIL -> WARN\n"
        "Consola: PASS\n"
        "Almacenamiento: WARN (degraded o no verificado)\n"
        "Audit journal TXT/JSONL: WARN\n"
        "Audit SQLite: UNAVAILABLE (uefi restricted diagnostic)\n"
        "Operational mode: restricted diagnostic\n"
        "Limitaciones: no implementado wifi/network remoto\n";
    const char *jsonl_pass = "{\"type\":\"self_test\",\"status\":\"pass\",\"audit\":\"hosted\"}\n";
    const char *jsonl_warn = "{\"type\":\"self_test\",\"status\":\"warn\",\"audit\":\"restricted\",\"sqlite_status\":\"unavailable\"}\n";
    static char txt_report[4096];
    static char jsonl_report[4096];
    size_t txt_len = 0U;
    size_t jsonl_len = 0U;

    resolve_audit_runtime_status(ctx, &rt);
    claim_count = vita_ir_claims_from_audit_runtime(&rt, claims, VITA_IR_AUDIT_CLAIM_MAX);

    append_text(txt_report, sizeof(txt_report), &txt_len,
                (ctx && ctx->boot_status.audit_ready) ? txt_pass : (rt.storage_status == VITA_STATUS_OK ? txt_warn_storage_ok : txt_warn_storage_degraded));
    append_text(jsonl_report, sizeof(jsonl_report), &jsonl_len,
                (ctx && ctx->boot_status.audit_ready) ? jsonl_pass : jsonl_warn);

    if (claim_count == 0U) {
        append_text(txt_report, sizeof(txt_report), &txt_len, "VitaIR-Tri runtime claims: unavailable\n");
        append_text(jsonl_report, sizeof(jsonl_report), &jsonl_len,
                    "{\"type\":\"vitair_claims\",\"status\":\"unavailable\"}\n");
    } else {
        append_text(txt_report, sizeof(txt_report), &txt_len, "VitaIR-Tri runtime claims:\n");

        for (i = 0; i < claim_count; ++i) {
            char txt_line[192];
            char jsonl_line[256];
            size_t txt_line_len = 0U;
            size_t jsonl_line_len = 0U;
            const char *claim = (claims[i].claim && claims[i].claim[0]) ? claims[i].claim : "unknown.claim";
            const char *symbol = vita_tri_to_symbol(claims[i].state);
            const char *state_num = vita_tri_to_json_number(claims[i].state);
            const char *severity = vita_ir_severity_to_string(claims[i].severity);

            if (!symbol || !symbol[0]) {
                symbol = "0";
            }
            if (!state_num || !state_num[0]) {
                state_num = "0";
            }
            if (!severity || !severity[0]) {
                severity = "warn";
            }

            txt_line[0] = '\0';
            append_text(txt_line, sizeof(txt_line), &txt_line_len, "- ");
            append_text(txt_line, sizeof(txt_line), &txt_line_len, claim);
            append_text(txt_line, sizeof(txt_line), &txt_line_len, ": ");
            append_text(txt_line, sizeof(txt_line), &txt_line_len, symbol);
            append_text(txt_line, sizeof(txt_line), &txt_line_len, " ");
            append_text(txt_line, sizeof(txt_line), &txt_line_len, severity);
            append_text(txt_line, sizeof(txt_line), &txt_line_len, "\n");
            append_text(txt_report, sizeof(txt_report), &txt_len, txt_line);

            jsonl_line[0] = '\0';
            append_text(jsonl_line, sizeof(jsonl_line), &jsonl_line_len,
                        "{\"type\":\"vitair_claim\",\"ir_version\":\"");
            append_text(jsonl_line, sizeof(jsonl_line), &jsonl_line_len, VITA_IR_VERSION);
            append_text(jsonl_line, sizeof(jsonl_line), &jsonl_line_len,
                        "\",\"claim\":\"");
            append_text(jsonl_line, sizeof(jsonl_line), &jsonl_line_len, claim);
            append_text(jsonl_line, sizeof(jsonl_line), &jsonl_line_len,
                        "\",\"state\":");
            append_text(jsonl_line, sizeof(jsonl_line), &jsonl_line_len, state_num);
            append_text(jsonl_line, sizeof(jsonl_line), &jsonl_line_len,
                        ",\"severity\":\"");
            append_text(jsonl_line, sizeof(jsonl_line), &jsonl_line_len, severity);
            append_text(jsonl_line, sizeof(jsonl_line), &jsonl_line_len,
                        "\"}\n");
            append_text(jsonl_report, sizeof(jsonl_report), &jsonl_len, jsonl_line);
        }
    }

    if (write_report_pair_verified(txt_path, txt_report, jsonl_path, jsonl_report)) {
        console_write_line("selftest: written");
        return;
    }

    console_write_line("selftest: failed");
}

vita_command_result_t command_handle_line(vita_command_context_t *ctx, const char *line) {
    char local[VITA_CONSOLE_LINE_MAX];
    const char *cmd;
    unsigned long i = 0;

    if (!ctx || !line) {
        return VITA_COMMAND_CONTINUE;
    }

    while (line[i] && (i + 1) < sizeof(local)) {
        local[i] = line[i];
        i++;
    }
    local[i] = '\0';

    trim_right_in_place(local);
    cmd = trim_left(local);

    if (!cmd[0]) {
        return VITA_COMMAND_CONTINUE;
    }
    session_transcript_log_user_input(cmd);
    session_transcript_log_command_executed(cmd);

    command_refresh_state(ctx);

    if (str_eq(cmd, "status")) {
        audit_emit_boot_event("COMMAND_STATUS", "status");
        console_guided_show_status(&ctx->console_state);
        show_status_vitair_summary(ctx);
        return VITA_COMMAND_CONTINUE;
    }

    if (str_eq(cmd, "hw")) {
        audit_emit_boot_event("COMMAND_HW", "hw");
        show_hw(ctx);
        return VITA_COMMAND_CONTINUE;
    }

    if (str_eq(cmd, "audit")) {
        audit_emit_boot_event("COMMAND_AUDIT", "audit");
        show_audit(ctx);
        return VITA_COMMAND_CONTINUE;
    }

    if (str_eq(cmd, "audit status") || str_eq(cmd, "audit report") ||
        str_eq(cmd, "audit gate") || str_eq(cmd, "audit readiness")) {
        handle_audit_readiness(ctx);
        return VITA_COMMAND_CONTINUE;
    }

    if (str_eq(cmd, "audit verify") || str_eq(cmd, "audit verify-chain") ||
        str_eq(cmd, "audit hash") || str_eq(cmd, "audit hash-chain")) {
        handle_audit_verify(ctx);
        return VITA_COMMAND_CONTINUE;
    }

    if (str_eq(cmd, "audit export") || str_eq(cmd, "audit verify export") ||
        str_eq(cmd, "audit export verify") || str_eq(cmd, "export audit verify")) {
        handle_audit_export(ctx);
        return VITA_COMMAND_CONTINUE;
    }

    if (str_eq(cmd, "audit events") || str_eq(cmd, "audit export events") ||
        str_eq(cmd, "export audit events")) {
        handle_audit_events_export(ctx);
        return VITA_COMMAND_CONTINUE;
    }

    if (str_eq(cmd, "audit sqlite") || str_eq(cmd, "audit sqlite summary") ||
        str_eq(cmd, "audit db") || str_eq(cmd, "audit db summary")) {
        handle_audit_sqlite_summary(ctx);
        return VITA_COMMAND_CONTINUE;
    }

    if (str_eq(cmd, "peers")) {
        audit_emit_boot_event("COMMAND_PEERS", "peers");
        show_peers(ctx);
        return VITA_COMMAND_CONTINUE;
    }

    if (str_eq(cmd, "proposals") || str_eq(cmd, "list")) {
        audit_emit_boot_event("COMMAND_PROPOSALS", "proposals");
        show_proposals(ctx);
        return VITA_COMMAND_CONTINUE;
    }

    if (str_eq(cmd, "emergency")) {
        ctx->console_state.mode = VITA_CONSOLE_MODE_EMERGENCY;
        show_emergency(ctx, 0);
        return VITA_COMMAND_CONTINUE;
    }

    if (str_eq(cmd, "helpme") || str_eq(cmd, "help") || str_eq(cmd, "menu")) {
        audit_emit_boot_event("COMMAND_HELPME", "helpme");
        console_guided_show_help();
        console_guided_show_menu();
        return VITA_COMMAND_CONTINUE;
    }

    if (str_eq(cmd, "clear") || str_eq(cmd, "cls")) {
        audit_emit_boot_event("COMMAND_CLEAR", "clear");
        console_clear_screen();
        show_refreshed_header(ctx);
        return VITA_COMMAND_CONTINUE;
    }

    if (starts_with(cmd, "approve ") || starts_with(cmd, "reject ")) {
        if (!ctx->proposal_ready) {
            console_write_line("Proposal engine unavailable / Motor de propuestas no disponible");
            return VITA_COMMAND_CONTINUE;
        }

        if (!proposal_handle_command(cmd)) {
            console_write_line("proposal command failed / comando de propuesta no ejecutado");
        }
        return VITA_COMMAND_CONTINUE;
    }

    if (ai_gateway_handle_command(cmd)) {
        return VITA_COMMAND_CONTINUE;
    }

    if (str_eq(cmd, "storage status")) {
        storage_show_status();
        show_storage_status_vitair_summary(ctx);
        return VITA_COMMAND_CONTINUE;
    }

    if (storage_handle_command(cmd)) {
        return VITA_COMMAND_CONTINUE;
    }

    if (editor_handle_command(cmd)) {
        return VITA_COMMAND_CONTINUE;
    }

    if (session_journal_handle_command(cmd)) {
        return VITA_COMMAND_CONTINUE;
    }

    if (str_eq(cmd, "export session")) {
        (void)session_export_write_report(ctx);
        return VITA_COMMAND_CONTINUE;
    }
    if (str_eq(cmd, "export session jsonl")) {
        (void)session_export_write_report(ctx);
        (void)session_export_write_jsonl(ctx);
        return VITA_COMMAND_CONTINUE;
    }

    if (str_eq(cmd, "export jsonl")) {
        (void)session_export_write_jsonl(ctx);
        return VITA_COMMAND_CONTINUE;
    }

    if (str_eq(cmd, "export vitair") || str_eq(cmd, "export vitair-state")) {
        handle_export_vitair(ctx);
        return VITA_COMMAND_CONTINUE;
    }

    if (str_eq(cmd, "diagnostic") || str_eq(cmd, "diag") || str_eq(cmd, "export diagnostic")) {
        handle_diagnostic_bundle(ctx);
        return VITA_COMMAND_CONTINUE;
    }

    if (str_eq(cmd, "export index") || str_eq(cmd, "export manifest") ||
        str_eq(cmd, "export list") || str_eq(cmd, "exports") ||
        str_eq(cmd, "storage export-index")) {
        handle_export_index();
        return VITA_COMMAND_CONTINUE;
    }

    if (str_eq(cmd, "selftest") || str_eq(cmd, "self-test") ||
        str_eq(cmd, "boot selftest") || str_eq(cmd, "boot self-test") ||
        str_eq(cmd, "checkup")) {
        handle_selftest(ctx);
        return VITA_COMMAND_CONTINUE;
    }

    if (str_eq(cmd, "shutdown") || str_eq(cmd, "exit")) {
        audit_emit_boot_event("COMMAND_SHUTDOWN", "shutdown");
        (void)session_journal_flush();
        console_write_line("Shutting down command session / Cerrando sesion de comandos");
        return VITA_COMMAND_SHUTDOWN;
    }

    show_emergency(ctx, cmd);
    return VITA_COMMAND_CONTINUE;
}

void command_loop_run(vita_command_context_t *ctx) {
    char line[VITA_CONSOLE_LINE_MAX];
    vita_command_result_t result;

    if (!ctx) {
        return;
    }

    console_write_line("Interactive console ready / Consola interactiva lista");
    console_write_line("Type helpme for commands / Escribe helpme para comandos");
    session_transcript_log_system_output("interactive_console_ready", false);
    audit_emit_boot_event("COMMAND_REPL_STARTED", "interactive command loop started");

    for (;;) {
        bool had_tab = false;
        bool completed = false;
        unsigned long i = 0;

        console_guided_prompt();

        if (!console_read_line(line, sizeof(line))) {
            console_write_line("Console input unavailable or closed / Entrada de consola no disponible o cerrada");
            audit_emit_boot_event("COMMAND_REPL_INPUT_CLOSED", "console input unavailable or closed");
            break;
        }
        while (line[i]) {
            if (line[i] == '\t') {
                had_tab = true;
                break;
            }
            i++;
        }
        if (had_tab) {
            completed = console_autocomplete_line(line, sizeof(line));
            if (!completed) {
                continue;
            }
        }
        console_pager_begin(VITA_CONSOLE_PAGE_LINES_DEFAULT);
        result = command_handle_line(ctx, line);
        console_pager_end();

        if (result == VITA_COMMAND_SHUTDOWN) {
            break;
        }
    }

    audit_emit_boot_event("COMMAND_REPL_STOPPED", "interactive command loop stopped");
}
