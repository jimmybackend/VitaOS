/*
 * kernel/console.c
 * Guided bilingual text console for F1A.
 */

#include <vita/audit.h>
#include <vita/console.h>
#include <vita/node.h>
#include <vita/proposal.h>

#ifdef VITA_HOSTED
#include <stdio.h>
#include <string.h>
#endif

static vita_console_write_fn g_console_writer = 0;


#ifdef VITA_HOSTED
static bool str_eq(const char *a, const char *b) {
    unsigned long i = 0;
    if (!a || !b) return false;
    while (a[i] && b[i]) {
        if (a[i] != b[i]) return false;
        i++;
    }
    return a[i] == b[i];
}

static bool starts_with(const char *s, const char *prefix) {
    unsigned long i = 0;
    if (!s || !prefix) return false;
    while (prefix[i]) {
        if (s[i] != prefix[i]) return false;
        i++;
    }
    return true;
}

#endif

static void u64_to_dec(uint64_t v, char out[32]) {
    char tmp[32];
    int i = 0;
    int j = 0;
    if (v == 0) {
        out[0] = '0';
        out[1] = '\0';
        return;
    }
    while (v > 0 && i < (int)sizeof(tmp) - 1) {
        tmp[i++] = (char)('0' + (v % 10));
        v /= 10;
    }
    while (i > 0) out[j++] = tmp[--i];
    out[j] = '\0';
}

void console_bind_writer(vita_console_write_fn writer) {
    g_console_writer = writer;
}

void console_early_init(void) {
    /* Early console writer is provided by architecture boot stage. */
}

void console_write_line(const char *text) {
    if (!g_console_writer || !text) {
        return;
    }
    g_console_writer(text);
}

void console_banner(const vita_boot_status_t *status) {
    console_write_line("VitaOS with AI / VitaOS con IA");
    console_write_line("Boot: OK");
    if (status && status->arch_name && status->arch_name[0]) {
        if (status->arch_name[0] == 'x') {
            console_write_line("Arch: x86_64");
        } else {
            console_write_line("Arch: unknown");
        }
    } else {
        console_write_line("Arch: unknown");
    }
    console_write_line((status && status->console_ready) ? "Console: OK" : "Console: MISSING");
    console_write_line((status && status->audit_ready) ? "Audit: READY" : "Audit: FAILED");
}

void console_show_hw_snapshot(const vita_hw_snapshot_t *hw) {
    char num[32];
    if (!hw) {
        console_write_line("Hardware snapshot unavailable / Snapshot de hardware no disponible");
        return;
    }
    console_write_line("HW Snapshot:");
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
    console_write_line("net_count:");
    u64_to_dec((uint64_t)hw->net_count, num);
    console_write_line(num);
    console_write_line("storage_count:");
    u64_to_dec((uint64_t)hw->storage_count, num);
    console_write_line(num);
    console_write_line("usb_count:");
    u64_to_dec((uint64_t)hw->usb_count, num);
    console_write_line(num);
    console_write_line("wifi_count:");
    u64_to_dec((uint64_t)hw->wifi_count, num);
    console_write_line(num);
}

static void show_main_menu(void) {
    console_write_line("==== Guided Console / Consola guiada ====");
    console_write_line("1) Emergency assistance / Asistencia de emergencia -> emergency | helpme");
    console_write_line("2) Hardware and system status / Estado de hardware y sistema -> status | hw");
    console_write_line("3) Cooperative nodes / Nodos cooperativos -> peers");
    console_write_line("4) Audit review / Revision de auditoria -> audit");
    console_write_line("5) Technical console / Consola tecnica -> proposals | approve <id> | reject <id>");
    console_write_line("Commands: tasks | tasks pending | replicate | help | shutdown");
}

void console_show_guided_welcome(const vita_handoff_t *handoff,
                                 const vita_boot_status_t *status,
                                 const vita_hw_snapshot_t *hw,
                                 bool ai_core_online) {
    char num[32];
    int pending = proposal_count_pending();
    vita_audit_runtime_t audit_rt;

    audit_get_runtime(&audit_rt);
    console_write_line("VitaOS with AI / VitaOS con IA");
    console_write_line("--- Guided startup / Inicio guiado ---");
    console_write_line((handoff && handoff->firmware_type == VITA_FIRMWARE_HOSTED)
                           ? "Boot state / Estado de arranque: hosted"
                           : "Boot state / Estado de arranque: uefi");
    console_write_line((status && status->audit_ready) ? "Audit state / Estado de auditoria: READY" : "Audit state / Estado de auditoria: FAILED");
    console_write_line((hw && hw->ram_bytes > 0) ? "Hardware detected / Hardware detectado: YES" : "Hardware detected / Hardware detectado: PARTIAL");
    console_write_line(ai_core_online ? "AI core / Nucleo IA: ONLINE" : "AI core / Nucleo IA: OFFLINE");

    console_write_line("Pending proposals / Propuestas pendientes:");
    u64_to_dec((uint64_t)pending, num);
    console_write_line(num);

    console_write_line("Audit events in boot / Eventos auditados en boot:");
    u64_to_dec(audit_rt.event_seq, num);
    console_write_line(num);

    show_main_menu();
    audit_emit_boot_event("GUIDED_CONSOLE_SHOWN", "guided console shown");
}

#ifdef VITA_HOSTED
static bool emergency_is_probable(const char *text) {
    if (!text || !text[0]) return false;
    return strstr(text, "help") || strstr(text, "emerg") || strstr(text, "sang") ||
           strstr(text, "dolor") || strstr(text, "fuego") || strstr(text, "rescue") ||
           strstr(text, "auxilio") || strstr(text, "accident");
}

static void show_structured_response(const char *input,
                                     const vita_boot_status_t *status,
                                     const vita_hw_snapshot_t *hw) {
    char num[32];
    bool probable = emergency_is_probable(input);
    int pending = proposal_count_pending();
    vita_audit_runtime_t audit_rt;

    audit_get_runtime(&audit_rt);

    console_write_line("1) Resumen entendido / Understood summary:");
    console_write_line(input && input[0] ? input : "No input / Sin entrada");

    console_write_line("2) Riesgos inmediatos / Immediate risks:");
    console_write_line(probable ? "Possible emergency detected; treat as high priority." : "No obvious emergency pattern; continue guided triage.");

    console_write_line("3) Preguntas criticas / Critical questions:");
    console_write_line("Location? Immediate danger? Is someone injured?");

    console_write_line("4) Acciones inmediatas sugeridas / Immediate suggested actions:");
    console_write_line(probable ? "Call local emergency services and keep communication channel open." : "Provide more details and monitor changes every minute.");

    console_write_line("5) Recursos del sistema disponibles / Available system resources:");
    console_write_line(status && status->audit_ready ? "Audit persistence: READY" : "Audit persistence: FAILED");
    console_write_line(hw && hw->ram_bytes > 0 ? "Hardware snapshot: READY" : "Hardware snapshot: PARTIAL");
    console_write_line("Pending proposals:");
    u64_to_dec((uint64_t)pending, num);
    console_write_line(num);

    console_write_line("6) Propuesta de accion del sistema / System action proposal:");
    console_write_line(probable ? "Open guided emergency mode and request approval for priority response workflow." : "Review hardware/audit status proposal before escalating emergency mode.");

    console_write_line("7) Estado de auditoria / Audit state:");
    console_write_line(audit_rt.persistent_ready ? "Persistent chain active" : "Persistent chain inactive");

    audit_emit_boot_event("STRUCTURED_RESPONSE_SHOWN", "structured response shown");
}
#endif

void console_guided_hosted_loop(const vita_handoff_t *handoff,
                                const vita_boot_status_t *status,
                                const vita_hw_snapshot_t *hw,
                                bool ai_core_online) {
#ifdef VITA_HOSTED
    char line[256];
    (void)handoff;
    console_show_guided_welcome(handoff, status, hw, ai_core_online);

    while (fgets(line, sizeof(line), stdin)) {
        size_t n = strlen(line);
        while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r')) line[--n] = '\0';

        node_core_poll();

        if (str_eq(line, "shutdown") || str_eq(line, "exit")) {
            audit_emit_boot_event("MENU_OPTION_SELECTED", "shutdown");
            console_write_line("Shutdown requested / Apagado solicitado");
            break;
        }

        if (str_eq(line, "help")) {
            audit_emit_boot_event("MENU_OPTION_SELECTED", "help");
            show_main_menu();
            continue;
        }

        if (str_eq(line, "status")) {
            audit_emit_boot_event("MENU_OPTION_SELECTED", "status");
            console_show_guided_welcome(handoff, status, hw, ai_core_online);
            continue;
        }

        if (str_eq(line, "hw")) {
            audit_emit_boot_event("MENU_OPTION_SELECTED", "hw");
            console_show_hw_snapshot(hw);
            continue;
        }

        if (str_eq(line, "audit")) {
            char num[32];
            vita_audit_runtime_t rt;
            audit_emit_boot_event("MENU_OPTION_SELECTED", "audit");
            audit_get_runtime(&rt);
            console_write_line("Audit runtime / Estado runtime auditoria:");
            console_write_line(rt.persistent_ready ? "READY" : "FAILED");
            console_write_line("boot_id:");
            console_write_line(rt.boot_id ? rt.boot_id : "");
            console_write_line("event_seq:");
            u64_to_dec(rt.event_seq, num);
            console_write_line(num);
            continue;
        }

        if (str_eq(line, "peers")) {
            audit_emit_boot_event("MENU_OPTION_SELECTED", "peers");
            console_write_line("Cooperative nodes / Nodos cooperativos:");
            node_core_show_peers();
            continue;
        }


        if (str_eq(line, "tasks")) {
            audit_emit_boot_event("MENU_OPTION_SELECTED", "tasks");
            node_core_show_tasks(false);
            continue;
        }

        if (str_eq(line, "tasks pending")) {
            audit_emit_boot_event("MENU_OPTION_SELECTED", "tasks pending");
            node_core_show_tasks(true);
            continue;
        }

        if (str_eq(line, "replicate")) {
            audit_emit_boot_event("MENU_OPTION_SELECTED", "replicate");
            if (node_core_request_audit_replication()) {
                console_write_line("replication request sent");
            } else {
                console_write_line("replication request failed");
            }
            continue;
        }

        if (str_eq(line, "helpme") || str_eq(line, "emergency") || starts_with(line, "emergency ")) {
            const char *text = "";
            audit_emit_boot_event("MENU_OPTION_SELECTED", "emergency");
            audit_emit_boot_event("EMERGENCY_SESSION_STARTED", "emergency session started");
            if (starts_with(line, "emergency ")) {
                text = line + 10;
            } else if (str_eq(line, "helpme")) {
                console_write_line("Describe the situation / Describe la situacion:");
                if (!fgets(line, sizeof(line), stdin)) {
                    break;
                }
                n = strlen(line);
                while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r')) line[--n] = '\0';
                text = line;
            }
            audit_emit_boot_event("EMERGENCY_INPUT_RECEIVED", text && text[0] ? text : "empty input");
            show_structured_response(text, status, hw);
            continue;
        }

        if (proposal_handle_command(line)) {
            audit_emit_boot_event("MENU_OPTION_SELECTED", "proposal command");
            continue;
        }

        console_write_line("Unknown command / Comando desconocido. Use: help");
    }
#else
    (void)handoff;
    (void)status;
    (void)hw;
    (void)ai_core_online;
    console_write_line("guided loop unavailable");
#endif
}
