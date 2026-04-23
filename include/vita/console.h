#ifndef VITA_CONSOLE_H
#define VITA_CONSOLE_H

#include <stdbool.h>
#include <stdint.h>

typedef void (*vita_console_write_fn)(const char *text);

typedef struct {
    const char *arch_name;
    bool console_ready;
    bool audit_ready;
} vita_boot_status_t;

void console_bind_writer(vita_console_write_fn writer);
void console_early_init(void);
void console_write_line(const char *text);
void console_banner(const vita_boot_status_t *status);

#endif
