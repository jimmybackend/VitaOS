/*
 * kernel/panic.c
 * Panic and fatal path handling.
 */

#include <vita/console.h>

void audit_emit_boot_event(const char *event_type, const char *summary);

static void panic_halt_forever(void) {
    for (;;) {
        __asm__ __volatile__("hlt");
    }
}

void panic_fatal(const char *reason) {
    audit_emit_boot_event("PANIC_FATAL", reason ? reason : "unknown");
    console_write_line("PANIC: fatal boot error");
    if (reason) {
        console_write_line(reason);
    }
    panic_halt_forever();
}
