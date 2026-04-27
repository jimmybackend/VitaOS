#ifndef VITA_CONSOLE_H
#define VITA_CONSOLE_H

#include <stdbool.h>
#include <stdint.h>

#define VITA_CONSOLE_LINE_MAX 128U

typedef void (*vita_console_write_fn)(const char *text);
typedef bool (*vita_console_read_line_fn)(char *out, unsigned long out_cap);

typedef struct {
    const char *arch_name;
    bool console_ready;
    bool audit_ready;
} vita_boot_status_t;

typedef enum {
    VITA_CONSOLE_MODE_BOOT = 0,
    VITA_CONSOLE_MODE_GUIDED,
    VITA_CONSOLE_MODE_TECHNICAL,
    VITA_CONSOLE_MODE_EMERGENCY,
    VITA_CONSOLE_MODE_AUDIT,
    VITA_CONSOLE_MODE_NODES
} vita_console_mode_t;

typedef struct {
    const char *arch_name;
    const char *boot_mode;
    const char *language_mode;
    bool console_ready;
    bool audit_ready;
    bool proposal_engine_ready;
    bool node_core_ready;
    uint32_t peer_count;
    uint32_t pending_proposal_count;
    vita_console_mode_t mode;
} vita_console_state_t;

void console_bind_writer(vita_console_write_fn writer);
void console_bind_reader(vita_console_read_line_fn reader);
void console_early_init(void);
void console_write_line(const char *text);
bool console_read_line(char *out, unsigned long out_cap);

void console_banner(const vita_boot_status_t *status);

void console_guided_show_welcome(const vita_console_state_t *state);
void console_guided_show_menu(void);
void console_guided_show_help(void);
void console_guided_show_status(const vita_console_state_t *state);
void console_guided_prompt(void);

#endif
