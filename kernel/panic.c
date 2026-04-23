/*
 * kernel/panic.c
 * Panic and fatal path handling.
 *
 * Every panic path must attempt to leave an audit trail if possible.
 */

#include <stdint.h>

void audit_emit_boot_event(const char *event_type, const char *summary);

static void panic_halt_forever(void) {
    for (;;) {
        /* TODO: architecture-specific halt instruction */
    }
}

void panic_fatal(const char *reason) {
    audit_emit_boot_event("PANIC_FATAL", reason ? reason : "unknown");
    /* TODO:
     * - print panic on early console
     * - flush buffered audit events if possible
     * - halt or reset depending on policy
     */
    panic_halt_forever();
}
