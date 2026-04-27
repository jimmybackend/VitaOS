/*
 * kernel/command_core.c
 * Shared guided command loop for hosted and UEFI/F1A-F1B.
 */

#include <vita/audit.h>
#include <vita/command.h>
#include <vita/node.h>
#include <vita/proposal.h>

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

static void console_write_i32(const char *label, int value) {
    char num[32];

    console_write_line(label);
    u64_to_dec((uint64_t)((value < 0) ? 0 : value), num);
    console_write_line(num);
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
        ctx->boot_status = *boot_status;
    } else {
        ctx->boot_status.arch_name = "unknown";
        ctx->boot_status.console_ready = false;
        ctx->boot_status.audit_ready = false;
    }

    if (hw_snapshot) {
        ctx->hw_snapshot = *hw_snapshot;
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
    console_write_line("Audit / Auditoria:");

    if (ctx && ctx->boot_status.audit_ready) {
        console_write_line("Audit SQLite: READY / Auditoria SQLite: LISTA");
        console_write_line("Persistent audit backend is active for this session.");
        console_write_line("La bitacora persistente esta activa para esta sesion.");
        return;
    }

    console_write_line("Audit SQLite: FAILED / Auditoria SQLite: FALLA");
    console_write_line("Restricted diagnostic mode is active.");
    console_write_line("Modo diagnostico restringido activo.");
    console_write_line("UEFI physical boot can still run local console commands, but it must not claim full operational mode until persistent audit exists.");
    console_write_line("En UEFI fisico puedes usar comandos locales, pero el modo operativo completo requiere auditoria persistente.");
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

    command_refresh_state(ctx);

    if (str_eq(cmd, "status")) {
        audit_emit_boot_event("COMMAND_STATUS", "status");
        console_guided_show_status(&ctx->console_state);
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

    if (str_eq(cmd, "shutdown") || str_eq(cmd, "exit")) {
        audit_emit_boot_event("COMMAND_SHUTDOWN", "shutdown");
        console_write_line("Shutting down command session / Cerrando sesion de comandos");
        return VITA_COMMAND_SHUTDOWN;
    }

    show_emergency(ctx, cmd);
    return VITA_COMMAND_CONTINUE;
}

void command_loop_run(vita_command_context_t *ctx) {
    char line[VITA_CONSOLE_LINE_MAX];

    if (!ctx) {
        return;
    }

    console_write_line("Interactive console ready / Consola interactiva lista");
    console_write_line("Type helpme for commands / Escribe helpme para comandos");
    audit_emit_boot_event("COMMAND_REPL_STARTED", "interactive command loop started");

    for (;;) {
        console_guided_prompt();

        if (!console_read_line(line, sizeof(line))) {
            console_write_line("Console input unavailable or closed / Entrada de consola no disponible o cerrada");
            audit_emit_boot_event("COMMAND_REPL_INPUT_CLOSED", "console input unavailable or closed");
            break;
        }

        if (command_handle_line(ctx, line) == VITA_COMMAND_SHUTDOWN) {
            break;
        }
    }

    audit_emit_boot_event("COMMAND_REPL_STOPPED", "interactive command loop stopped");
}
