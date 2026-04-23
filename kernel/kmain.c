/*
 * kernel/kmain.c
 * First executable vertical slice entry + audit gate.
 */

#include <stdbool.h>
#include <stdint.h>

#include <vita/boot.h>
#include <vita/console.h>

void panic_fatal(const char *reason);
void audit_early_buffer_init(void);
bool audit_init_persistent_backend(const vita_handoff_t *handoff);
void audit_emit_boot_event(const char *event_type, const char *summary);

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
    vita_boot_status_t status = {
        .arch_name = "x86_64",
        .console_ready = true,
        .audit_ready = false,
    };

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
    console_banner(&status);

    if (handoff->firmware_type == VITA_FIRMWARE_HOSTED) {
        return;
    }

    for (;;) {
        __asm__ __volatile__("hlt");
    }
}
