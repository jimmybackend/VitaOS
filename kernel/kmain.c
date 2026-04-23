/*
 * kernel/kmain.c
 * First executable vertical slice entry + audit gate.
 */

#include <stdbool.h>

#include <vita/audit.h>
#include <vita/boot.h>
#include <vita/console.h>
#include <vita/hw.h>
#include <vita/node.h>
#include <vita/proposal.h>

void panic_fatal(const char *reason);

static void mem_zero(void *ptr, unsigned long n) {
    unsigned char *p = (unsigned char *)ptr;
    for (unsigned long i = 0; i < n; ++i) p[i] = 0;
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

void kmain(const vita_handoff_t *handoff) {
    vita_boot_status_t status;
    vita_hw_snapshot_t hw;
    bool ai_core_online = false;

    mem_zero(&status, sizeof(status));
    mem_zero(&hw, sizeof(hw));
    status.arch_name = "x86_64";
    status.console_ready = true;
    status.audit_ready = false;

    console_early_init();
    audit_early_buffer_init();
    audit_emit_boot_event("BOOT_STARTED", "boot started");

    if (!handoff_valid(handoff)) {
        panic_fatal("invalid boot handoff");
        return;
    }

    audit_emit_boot_event("HANDOFF_TO_KMAIN", "handoff to kmain");
    audit_emit_boot_event("CONSOLE_READY", "console ready");

    if (handoff->arch_name) {
        status.arch_name = handoff->arch_name;
    }

    if (!audit_init_persistent_backend(handoff)) {
        console_write_line("Audit: FAILED");
        panic_fatal("persistent audit backend required");
        return;
    }

    status.audit_ready = true;
    audit_emit_boot_event("AUDIT_BACKEND_READY", "audit backend ready");

    audit_emit_boot_event("HW_DISCOVERY_STARTED", "hardware discovery started");
    if (hw_discovery_run(handoff, &hw)) {
        audit_emit_boot_event("HW_DISCOVERY_COMPLETED", "hardware discovery completed");
        if (audit_persist_hardware_snapshot(&hw)) {
            audit_emit_boot_event("HW_SNAPSHOT_PERSISTED", "hardware snapshot persisted");
        }
    }

    node_core_start(handoff);

    proposal_generate_initial(handoff, &hw, status.audit_ready);
    ai_core_online = true;

    console_banner(&status);
    console_show_guided_welcome(handoff, &status, &hw, ai_core_online);

    if (handoff->firmware_type == VITA_FIRMWARE_HOSTED) {
        console_guided_hosted_loop(handoff, &status, &hw, ai_core_online);
        return;
    }

    for (;;) {
        __asm__ __volatile__("hlt");
    }
}
