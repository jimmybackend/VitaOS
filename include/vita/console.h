#ifndef VITA_CONSOLE_H
#define VITA_CONSOLE_H

#include <stdbool.h>
#include <stdint.h>

#define VITA_CONSOLE_LINE_MAX 128U
#define VITA_CONSOLE_HISTORY_MAX 8U
#define VITA_CONSOLE_PAGE_LINES_DEFAULT 22U

typedef void (*vita_console_write_fn)(const char *text);
typedef void (*vita_console_write_raw_fn)(const char *text);
typedef bool (*vita_console_read_line_fn)(char *out, unsigned long out_cap);
typedef void (*vita_console_clear_fn)(void);

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

typedef enum {
    VITA_CONSOLE_STYLE_DEFAULT = 0,
    VITA_CONSOLE_STYLE_EN,
    VITA_CONSOLE_STYLE_ES,
    VITA_CONSOLE_STYLE_ERROR,
    VITA_CONSOLE_STYLE_OK,
    VITA_CONSOLE_STYLE_WARNING,
    VITA_CONSOLE_STYLE_EDITOR_TEXT
} vita_console_style_t;

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
void console_bind_raw_writer(vita_console_write_raw_fn writer);
void console_bind_reader(vita_console_read_line_fn reader);
void console_bind_clear(vita_console_clear_fn clear_fn);

void console_early_init(void);
void console_write_line(const char *text);
void console_write_raw(const char *text);
void console_set_style(vita_console_style_t style);
void console_reset_style(void);
void console_write_en(const char *text);
void console_write_es(const char *text);
void console_write_error(const char *text);
void console_write_ok(const char *text);
void console_write_warning(const char *text);
void console_write_editor_text(const char *text);
bool console_read_line(char *out, unsigned long out_cap);
void console_clear_screen(void);

void console_pager_begin(unsigned long page_lines);
void console_pager_end(void);

void console_banner(const vita_boot_status_t *status);

void console_guided_show_welcome(const vita_console_state_t *state);
void console_guided_show_menu(void);
void console_guided_show_help(void);
void console_guided_show_status(const vita_console_state_t *state);
void console_prompt_begin(void);
void console_prompt_end(void);
void console_input_style_begin(void);
void console_input_style_end(void);
void console_guided_prompt(void);

#endif
