/*
 * kernel/kmain.c
 * F1A/F1B integrated boot flow:
 * - handoff validation
 * - audit gate
 * - hardware discovery
 * - guided console
 * - proposal engine boot integration
 * - minimal hosted VitaNet start
 * - shared hosted/UEFI command loop
 */

#include <stdbool.h>
#include <stdint.h>

#include <vita/audit.h>
#include <vita/boot.h>
#include <vita/command.h>
#include <vita/console.h>
#include <vita/hw.h>
#include <vita/node.h>
#include <vita/proposal.h>
#include <vita/session_journal.h>
#include <vita/storage.h>

void panic_fatal(const char *reason);

static void mem_zero(void *ptr, unsigned long n) {
    unsigned char *p = (unsigned char *)ptr;
    for (unsigned long i = 0; i < n; ++i) {
        p[i] = 0;
    }
}

static bool handoff_valid(const vita_handoff_t *handoff) {
    if (!handoff) {
        return false;
    }
    if (handoff->magic != VITA_BOOT_MAGIC) {
        return false;
    }
    if (handoff->size < sizeof(vita_handoff_t)) {
        return false;
    }
    return true;
}

static void u64_to_dec(uint64_t v, char out[32]) {
    char tmp[32];
    int i = 0;
    int j = 0;

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

static void console_write_count(const char *label, int value) {
    char num[32];

    console_write_line(label);
    u64_to_dec((uint64_t)((value < 0) ? 0 : value), num);
    console_write_line(num);
}

static void console_show_hw(const vita_hw_snapshot_t *hw) {
    char num[32];

    if (!hw) {
        return;
    }

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

    console_write_count("display_count:", hw->display_count);
    console_write_count("keyboard_count:", hw->keyboard_count);
    console_write_count("mouse_count:", hw->mouse_count);
    console_write_count("audio_count:", hw->audio_count);
    console_write_count("microphone_count:", hw->microphone_count);

    console_write_count("net_count:", hw->net_count);
    console_write_count("ethernet_count:", hw->ethernet_count);
    console_write_count("wifi_count:", hw->wifi_count);

    console_write_count("storage_count:", hw->storage_count);
    console_write_count("usb_count:", hw->usb_count);
    console_write_count("usb_controller_count:", hw->usb_controller_count);

    console_write_line("detected_at_unix:");
    u64_to_dec(hw->detected_at_unix, num);
    console_write_line(num);
}

static void console_fill_state(vita_console_state_t *state,
                               const vita_handoff_t *handoff,
                               const vita_boot_status_t *boot_status,
                               bool proposal_ready,
                               bool node_ready) {
    if (!state) {
        return;
    }

    mem_zero(state, sizeof(*state));

    state->arch_name = (boot_status && boot_status->arch_name)
        ? boot_status->arch_name
        : "unknown";
    state->boot_mode = boot_mode_name(handoff);
    state->language_mode = "es,en";
    state->console_ready = boot_status ? boot_status->console_ready : false;
    state->audit_ready = boot_status ? boot_status->audit_ready : false;
    state->proposal_engine_ready = proposal_ready;
    state->node_core_ready = node_ready;
    state->peer_count = (uint32_t)((node_ready) ? node_core_peer_count() : 0);
    state->pending_proposal_count = (uint32_t)((node_ready) ? node_core_pending_proposal_count() : 0);
    state->mode = state->audit_ready ? VITA_CONSOLE_MODE_GUIDED : VITA_CONSOLE_MODE_AUDIT;
}

void kmain(const vita_handoff_t *handoff) {
    vita_boot_status_t boot_status;
    vita_hw_snapshot_t hw;
    vita_console_state_t console_state;
    vita_command_context_t command_ctx;
    bool hw_ready = false;
    bool proposal_ready = false;
    bool node_ready = false;

    mem_zero(&boot_status, sizeof(boot_status));
    mem_zero(&hw, sizeof(hw));
    mem_zero(&console_state, sizeof(console_state));
    mem_zero(&command_ctx, sizeof(command_ctx));

    boot_status.arch_name = "unknown";
    boot_status.console_ready = true;
    boot_status.audit_ready = false;

    console_early_init();
    audit_early_buffer_init();
    audit_emit_boot_event("BOOT_STARTED", "boot started");

    if (!handoff_valid(handoff)) {
        panic_fatal("invalid boot handoff");
        return;
    }

    if (handoff->arch_name) {
        boot_status.arch_name = handoff->arch_name;
    }

    audit_emit_boot_event("HANDOFF_TO_KMAIN", "handoff to kmain");
    audit_emit_boot_event("CONSOLE_READY", "console ready");

    if (audit_init_persistent_backend(handoff)) {
        boot_status.audit_ready = true;
        audit_emit_boot_event("AUDIT_BACKEND_READY", "audit backend ready");
    } else {
        boot_status.audit_ready = false;
        console_write_line("Audit persistence unavailable.");
        console_write_line("Entering restricted diagnostic mode / Entrando en modo diagnostico restringido.");
    }

    audit_emit_boot_event("HW_DISCOVERY_STARTED", "hardware discovery started");
    if (hw_discovery_run(handoff, &hw)) {
        hw_ready = true;
        audit_emit_boot_event("HW_DISCOVERY_COMPLETED", "hardware discovery completed");

        if (boot_status.audit_ready && audit_persist_hardware_snapshot(&hw)) {
            audit_emit_boot_event("HW_SNAPSHOT_PERSISTED", "hardware snapshot persisted");
        }

        console_show_hw(&hw);
    }

    if (storage_init(handoff)) {
        audit_emit_boot_event("STORAGE_READY", "storage initialized");
        if (storage_bootstrap_persistent_tree()) {
            audit_emit_boot_event("STORAGE_BOOTSTRAP_VERIFIED", "storage bootstrap verified");
        } else {
            audit_emit_boot_event("STORAGE_BOOTSTRAP_FAILED", storage_last_error());
            console_write_line("storage bootstrap: failed");
            console_write_line(storage_last_error());
        }
    } else {
        audit_emit_boot_event("STORAGE_UNAVAILABLE", "storage not initialized");
    }

    if (session_journal_init()) {
        audit_emit_boot_event("SESSION_JOURNAL_READY", "session journal initialized");
    } else {
        audit_emit_boot_event("SESSION_JOURNAL_UNAVAILABLE", "session journal not active");
    }

    console_banner(&boot_status);

    /*
     * Keep proposals available in memory even in restricted UEFI diagnostic
     * mode. The audit backend persists them only when available.
     */
    proposal_generate_initial(handoff, hw_ready ? &hw : 0, boot_status.audit_ready);
    proposal_ready = true;
    audit_emit_boot_event(
        boot_status.audit_ready ? "PROPOSAL_ENGINE_READY" : "PROPOSAL_ENGINE_RESTRICTED_READY",
        boot_status.audit_ready ? "initial proposals generated" : "initial proposals generated in restricted mode");

    if (boot_status.audit_ready && handoff->firmware_type == VITA_FIRMWARE_HOSTED) {
        node_ready = node_core_start(handoff);
        if (node_ready) {
            audit_emit_boot_event("NODE_CORE_READY", "hosted node core ready");
        } else {
            audit_emit_boot_event("NODE_CORE_LIMITED", "hosted node core limited");
        }
    }

    console_fill_state(&console_state, handoff, &boot_status, proposal_ready, node_ready);

    console_guided_show_welcome(&console_state);
    console_guided_show_menu();
    console_guided_show_status(&console_state);

    if (!boot_status.audit_ready) {
        console_write_line("Operational mode blocked / Modo operativo bloqueado");
        console_write_line("Persistent audit is required before full guided operation.");
        console_write_line("La auditoria persistente es obligatoria antes del modo operativo completo.");
        console_write_line("Local diagnostic commands remain available / Los comandos locales de diagnostico siguen disponibles.");
    } else {
        proposal_show_all();

        if (node_core_pending_proposal_count() > 0) {
            node_core_show_link_proposal();
        }

        if (node_core_peer_count() > 0) {
            node_core_show_peers();
        }
    }

    command_context_init(&command_ctx,
                         handoff,
                         &boot_status,
                         hw_ready ? &hw : 0,
                         hw_ready,
                         proposal_ready,
                         node_ready);
    command_loop_run(&command_ctx);

    if (handoff->firmware_type == VITA_FIRMWARE_HOSTED) {
        return;
    }

    for (;;) {
        __asm__ __volatile__("hlt");
    }
}
