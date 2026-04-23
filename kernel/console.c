/*
 * kernel/console.c
 * Early text console bindings and boot banner.
 */

#include <vita/console.h>

static vita_console_write_fn g_console_writer = 0;

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
