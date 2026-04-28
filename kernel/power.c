/*
 * kernel/power.c
 * Safe staged restart/shutdown flow for VitaOS F1A/F1B.
 *
 * The goal is not to pretend we have a full power manager yet. This module
 * centralizes the request path, records the intent, flushes the visible
 * session journal, and then uses UEFI ResetSystem when available.
 */

#include <stdint.h>

#include <vita/audit.h>
#include <vita/boot.h>
#include <vita/console.h>
#include <vita/power.h>
#include <vita/session_journal.h>
#include <vita/storage.h>

#ifndef VITA_HOSTED
#define EFI_SUCCESS 0ULL
#define EFI_RESET_COLD 0U
#define EFI_RESET_SHUTDOWN 2U

typedef uint16_t char16_t;
typedef uint64_t efi_status_t;

typedef struct {
    uint64_t signature;
    uint32_t revision;
    uint32_t header_size;
    uint32_t crc32;
    uint32_t reserved;
} efi_table_header_t;

typedef struct efi_runtime_services efi_runtime_services_t;

typedef struct {
    efi_table_header_t hdr;
    char16_t *firmware_vendor;
    uint32_t firmware_revision;
    uint32_t _pad0;
    void *console_in_handle;
    void *con_in;
    void *console_out_handle;
    void *con_out;
    void *standard_error_handle;
    void *std_err;
    efi_runtime_services_t *runtime_services;
    void *boot_services;
    uint64_t number_of_table_entries;
    void *configuration_table;
} efi_system_table_t;

struct efi_runtime_services {
    efi_table_header_t hdr;
    void *get_time;
    void *set_time;
    void *get_wakeup_time;
    void *set_wakeup_time;
    void *set_virtual_address_map;
    void *convert_pointer;
    void *get_variable;
    void *get_next_variable_name;
    void *set_variable;
    void *get_next_high_mono_count;
    void (*reset_system)(uint32_t reset_type,
                         efi_status_t reset_status,
                         uint64_t data_size,
                         void *reset_data);
    void *update_capsule;
    void *query_capsule_capabilities;
    void *query_variable_info;
};
#endif

static const char *power_action_name(vita_power_action_t action) {
    switch (action) {
        case VITA_POWER_ACTION_SHUTDOWN:
            return "shutdown";
        case VITA_POWER_ACTION_RESTART:
            return "restart";
        default:
            return "none";
    }
}

static const char *boot_mode_name(const vita_command_context_t *ctx) {
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

static bool storage_ready_now(void) {
    vita_storage_status_t st;

    storage_get_status(&st);
    return st.initialized && st.writable;
}

static void power_preflight(vita_power_action_t action, const vita_command_context_t *ctx) {
    const char *name = power_action_name(action);

    (void)ctx;

    console_write_line("Power request / Solicitud de energia:");
    console_write_line(name);
    console_write_line("Saving visible session state / Guardando estado visible de sesion...");

    audit_emit_boot_event("POWER_REQUESTED", name);
    session_journal_log_system("POWER_REQUESTED", name);

    if (session_journal_is_active()) {
        session_journal_log_system("POWER_FLUSH_STARTED", name);
        if (session_journal_flush()) {
            console_write_line("Journal flush: OK / Bitacora guardada: OK");
            audit_emit_boot_event("POWER_JOURNAL_FLUSHED", name);
        } else {
            console_write_line("Journal flush: FAILED / Bitacora no guardada completamente");
            audit_emit_boot_event("POWER_JOURNAL_FLUSH_FAILED", name);
        }
    } else {
        console_write_line("Journal: inactive / Bitacora: inactiva");
        audit_emit_boot_event("POWER_JOURNAL_INACTIVE", name);
    }

    if (storage_ready_now()) {
        console_write_line("Storage: writable / Almacenamiento: escribible");
    } else {
        console_write_line("Storage: not writable / Almacenamiento: no escribible");
    }

    console_write_line("Remote AI transport: staged / Transporte IA remoto: pendiente");
    console_write_line("No active remote AI job is being waited on in this milestone.");
    console_write_line("No hay tarea remota de IA activa que esperar en este hito.");
}

#ifndef VITA_HOSTED
static bool power_uefi_reset(vita_power_action_t action, const vita_command_context_t *ctx) {
    efi_system_table_t *st;
    efi_runtime_services_t *rt;
    uint32_t reset_type;

    if (!ctx || !ctx->handoff || ctx->handoff->firmware_type != VITA_FIRMWARE_UEFI) {
        return false;
    }

    st = (efi_system_table_t *)ctx->handoff->uefi_system_table;
    if (!st || !st->runtime_services) {
        console_write_line("UEFI RuntimeServices unavailable / RuntimeServices UEFI no disponible");
        audit_emit_boot_event("POWER_UEFI_RUNTIME_MISSING", power_action_name(action));
        return false;
    }

    rt = st->runtime_services;
    if (!rt->reset_system) {
        console_write_line("UEFI ResetSystem unavailable / ResetSystem UEFI no disponible");
        audit_emit_boot_event("POWER_UEFI_RESET_MISSING", power_action_name(action));
        return false;
    }

    reset_type = (action == VITA_POWER_ACTION_RESTART) ? EFI_RESET_COLD : EFI_RESET_SHUTDOWN;

    console_write_line((action == VITA_POWER_ACTION_RESTART)
        ? "Restarting with UEFI ResetSystem / Reiniciando con UEFI ResetSystem"
        : "Powering off with UEFI ResetSystem / Apagando con UEFI ResetSystem");

    audit_emit_boot_event("POWER_UEFI_RESET_SYSTEM", power_action_name(action));
    session_journal_log_system("POWER_UEFI_RESET_SYSTEM", power_action_name(action));
    (void)session_journal_flush();

    rt->reset_system(reset_type, EFI_SUCCESS, 0, 0);

    console_write_line("UEFI ResetSystem returned unexpectedly / ResetSystem regreso inesperadamente");
    audit_emit_boot_event("POWER_UEFI_RESET_RETURNED", power_action_name(action));
    return false;
}
#endif

void power_show_status(const vita_command_context_t *ctx) {
    console_write_line("Power / Energia:");
    console_write_line("stage: safe F1A/F1B power request flow");
    console_write_line("boot_mode:");
    console_write_line(boot_mode_name(ctx));
    console_write_line("commands:");
    console_write_line("restart | reboot");
    console_write_line("shutdown | poweroff | exit");
    console_write_line("behavior:");
    console_write_line("- flush visible session journal when active");
    console_write_line("- record power intent in audit buffer/journal");
    console_write_line("- UEFI uses ResetSystem when firmware exposes it");
    console_write_line("- hosted shutdown exits the command loop");
}

bool power_request(vita_power_action_t action, const vita_command_context_t *ctx) {
    if (action != VITA_POWER_ACTION_SHUTDOWN && action != VITA_POWER_ACTION_RESTART) {
        console_write_line("power: invalid action / accion invalida");
        return false;
    }

    power_preflight(action, ctx);

#ifdef VITA_HOSTED
    if (action == VITA_POWER_ACTION_RESTART) {
        console_write_line("Hosted restart is not implemented; command loop remains active.");
        console_write_line("Reinicio hosted no implementado; la consola sigue activa.");
        audit_emit_boot_event("POWER_HOSTED_RESTART_UNSUPPORTED", power_action_name(action));
        session_journal_log_system("POWER_HOSTED_RESTART_UNSUPPORTED", power_action_name(action));
        (void)session_journal_flush();
        return false;
    }

    console_write_line("Hosted shutdown: leaving command loop / Cerrando consola hosted");
    audit_emit_boot_event("POWER_HOSTED_SHUTDOWN", power_action_name(action));
    session_journal_log_system("POWER_HOSTED_SHUTDOWN", power_action_name(action));
    (void)session_journal_flush();
    return true;
#else
    if (power_uefi_reset(action, ctx)) {
        return true;
    }

    if (action == VITA_POWER_ACTION_SHUTDOWN) {
        console_write_line("Shutdown fallback: stopping command loop and entering halt.");
        console_write_line("Apagado alterno: se detiene consola y se entra en halt.");
        audit_emit_boot_event("POWER_SHUTDOWN_HALT_FALLBACK", power_action_name(action));
        session_journal_log_system("POWER_SHUTDOWN_HALT_FALLBACK", power_action_name(action));
        (void)session_journal_flush();
        return true;
    }

    console_write_line("Restart failed; command loop remains active.");
    console_write_line("Reinicio fallido; la consola sigue activa.");
    return false;
#endif
}

bool power_handle_command(const char *cmd, const vita_command_context_t *ctx) {
    if (!cmd || !cmd[0]) {
        return false;
    }

    if ((cmd[0] == 'p' && cmd[1] == 'o' && cmd[2] == 'w' && cmd[3] == 'e' && cmd[4] == 'r' && cmd[5] == '\0') ||
        (cmd[0] == 'p' && cmd[1] == 'o' && cmd[2] == 'w' && cmd[3] == 'e' && cmd[4] == 'r' && cmd[5] == ' ')) {
        power_show_status(ctx);
        return true;
    }

    return false;
}
