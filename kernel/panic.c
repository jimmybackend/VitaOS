/*
 * kernel/panic.c
 * Panic and fatal path handling for the current slice.
 */

#include <vita/audit.h>
#include <vita/console.h>

#ifdef VITA_HOSTED
#include <stdlib.h>
#endif

static void panic_halt_forever(void) {
    for (;;) {
        __asm__ __volatile__("hlt");
    }
}

void panic_fatal(const char *reason) {
    audit_emit_boot_event("PANIC_FATAL", reason ? reason : "unknown");
    console_write_line("PANIC: fatal boot error");

    if (reason && reason[0]) {
        console_write_line(reason);
    } else {
        console_write_line("unknown");
    }

#ifdef VITA_HOSTED
    exit(1);
#else
    panic_halt_forever();
#endif
}
