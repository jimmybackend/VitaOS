/*
 * arch/x86_64/boot/uefi_entry.c
 * UEFI x86_64 entrypoint with explicit handoff to kmain.
 */

#include <stddef.h>
#include <stdint.h>

#include <vita/boot.h>
#include <vita/console.h>

#include "uefi_splash.h"

#define EFI_SUCCESS 0ULL
#define EFI_SCAN_UP 0x0001U
#define EFI_SCAN_DOWN 0x0002U

typedef uint64_t efi_status_t;
typedef void *efi_handle_t;
typedef void *efi_event_t;
typedef uint16_t char16_t;

typedef struct efi_simple_text_input_protocol efi_simple_text_input_protocol_t;
typedef struct efi_simple_text_output_protocol efi_simple_text_output_protocol_t;
typedef struct efi_boot_services efi_boot_services_t;

typedef struct {
    uint64_t signature;
    uint32_t revision;
    uint32_t header_size;
    uint32_t crc32;
    uint32_t reserved;
} efi_table_header_t;

typedef struct {
    uint16_t scan_code;
    char16_t unicode_char;
} efi_input_key_t;

struct efi_simple_text_input_protocol {
    efi_status_t (*reset)(efi_simple_text_input_protocol_t *self, uint8_t extended_verification);
    efi_status_t (*read_key_stroke)(efi_simple_text_input_protocol_t *self, efi_input_key_t *key);
    efi_event_t wait_for_key;
};

struct efi_simple_text_output_protocol {
    efi_status_t (*reset)(efi_simple_text_output_protocol_t *self, uint8_t extended_verification);
    efi_status_t (*output_string)(efi_simple_text_output_protocol_t *self, char16_t *string);
    efi_status_t (*test_string)(efi_simple_text_output_protocol_t *self, char16_t *string);
    efi_status_t (*query_mode)(efi_simple_text_output_protocol_t *self,
                               uint64_t mode_number,
                               uint64_t *columns,
                               uint64_t *rows);
    efi_status_t (*set_mode)(efi_simple_text_output_protocol_t *self, uint64_t mode_number);
    efi_status_t (*set_attribute)(efi_simple_text_output_protocol_t *self, uint64_t attribute);
    efi_status_t (*clear_screen)(efi_simple_text_output_protocol_t *self);
    efi_status_t (*set_cursor_position)(efi_simple_text_output_protocol_t *self,
                                         uint64_t column,
                                         uint64_t row);
    efi_status_t (*enable_cursor)(efi_simple_text_output_protocol_t *self, uint8_t visible);
    void *mode;
};

struct efi_boot_services {
    efi_table_header_t hdr;
    void *raise_tpl;
    void *restore_tpl;
    void *allocate_pages;
    void *free_pages;
    void *get_memory_map;
    void *allocate_pool;
    void *free_pool;
    void *create_event;
    void *set_timer;
    efi_status_t (*wait_for_event)(uint64_t number_of_events, efi_event_t *event, uint64_t *index);
    void *signal_event;
    void *close_event;
    efi_status_t (*check_event)(efi_event_t event);
    void *install_protocol_interface;
    void *reinstall_protocol_interface;
    void *uninstall_protocol_interface;
    void *handle_protocol;
    void *reserved;
    void *register_protocol_notify;
    void *locate_handle;
    void *locate_device_path;
    void *install_configuration_table;
    void *load_image;
    void *start_image;
    void *exit;
    void *unload_image;
    void *exit_boot_services;
    void *get_next_monotonic_count;
    efi_status_t (*stall)(uint64_t microseconds);
};

typedef struct {
    efi_table_header_t hdr;
    char16_t *firmware_vendor;
    uint32_t firmware_revision;
    uint32_t _pad0;
    efi_handle_t console_in_handle;
    efi_simple_text_input_protocol_t *con_in;
    efi_handle_t console_out_handle;
    efi_simple_text_output_protocol_t *con_out;
    efi_handle_t standard_error_handle;
    efi_simple_text_output_protocol_t *std_err;
    void *runtime_services;
    efi_boot_services_t *boot_services;
    uint64_t number_of_table_entries;
    void *configuration_table;
} efi_system_table_t;

void kmain(const vita_handoff_t *handoff);

static efi_system_table_t *g_st = 0;
static char g_history[VITA_CONSOLE_HISTORY_MAX][VITA_CONSOLE_LINE_MAX];
static unsigned long g_history_count = 0;

static void ascii_to_char16(const char *src, char16_t *dst, size_t dst_cap, uint8_t append_newline) {
    size_t i = 0;

    if (!dst || dst_cap == 0) {
        return;
    }

    if (!src) {
        dst[0] = 0;
        return;
    }

    while (src[i] != '\0' && i + 3 < dst_cap) {
        dst[i] = (char16_t)(uint8_t)src[i];
        i++;
    }

    if (append_newline && i + 2 < dst_cap) {
        dst[i++] = '\r';
        dst[i++] = '\n';
    }

    dst[i] = 0;
}

static void uefi_output_raw(const char *text) {
    char16_t buffer[256];

    if (!g_st || !g_st->con_out || !g_st->con_out->output_string) {
        return;
    }

    ascii_to_char16(text, buffer, sizeof(buffer) / sizeof(buffer[0]), 0);
    g_st->con_out->output_string(g_st->con_out, buffer);
}

static void uefi_console_write(const char *text) {
    char16_t buffer[256];

    if (!g_st || !g_st->con_out || !g_st->con_out->output_string) {
        return;
    }

    ascii_to_char16(text, buffer, sizeof(buffer) / sizeof(buffer[0]), 1);
    g_st->con_out->output_string(g_st->con_out, buffer);
}

static void uefi_console_write_raw(const char *text) {
    uefi_output_raw(text);
}

static void uefi_console_clear(void) {
    if (g_st && g_st->con_out && g_st->con_out->clear_screen) {
        g_st->con_out->clear_screen(g_st->con_out);
        return;
    }

    uefi_output_raw("\r\n\r\n\r\n");
}

static void uefi_echo_char(char c) {
    char text[2];

    text[0] = c;
    text[1] = '\0';
    uefi_output_raw(text);
}

static void uefi_poll_delay(void) {
    volatile unsigned long i;

    if (g_st && g_st->boot_services && g_st->boot_services->stall) {
        g_st->boot_services->stall(10000ULL);
        return;
    }

    for (i = 0; i < 200000UL; ++i) {
        __asm__ __volatile__("pause");
    }
}

static bool uefi_try_read_key(efi_input_key_t *key) {
    if (!key || !g_st || !g_st->con_in || !g_st->con_in->read_key_stroke) {
        return false;
    }

    return g_st->con_in->read_key_stroke(g_st->con_in, key) == EFI_SUCCESS;
}

static bool uefi_allowed_input_char(char c) {
    if (c >= 'a' && c <= 'z') {
        return true;
    }
    if (c >= 'A' && c <= 'Z') {
        return true;
    }
    if (c >= '0' && c <= '9') {
        return true;
    }
    if (c == ' ' || c == '-' || c == '_' || c == '.' || c == '/') {
        return true;
    }
    return false;
}

static void uefi_copy_text(char *dst, unsigned long cap, const char *src) {
    unsigned long i = 0;

    if (!dst || cap == 0) {
        return;
    }

    if (!src) {
        dst[0] = '\0';
        return;
    }

    while (src[i] && (i + 1) < cap) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static unsigned long uefi_strlen(const char *s) {
    unsigned long n = 0;

    if (!s) {
        return 0;
    }

    while (s[n]) {
        n++;
    }

    return n;
}

static void uefi_history_push(const char *line) {
    unsigned long i;

    if (!line || !line[0]) {
        return;
    }

    if (g_history_count > 0) {
        if (uefi_strlen(g_history[g_history_count - 1]) == uefi_strlen(line)) {
            bool same = true;
            for (i = 0; line[i] || g_history[g_history_count - 1][i]; ++i) {
                if (line[i] != g_history[g_history_count - 1][i]) {
                    same = false;
                    break;
                }
            }
            if (same) {
                return;
            }
        }
    }

    if (g_history_count < VITA_CONSOLE_HISTORY_MAX) {
        uefi_copy_text(g_history[g_history_count], VITA_CONSOLE_LINE_MAX, line);
        g_history_count++;
        return;
    }

    for (i = 1; i < VITA_CONSOLE_HISTORY_MAX; ++i) {
        uefi_copy_text(g_history[i - 1], VITA_CONSOLE_LINE_MAX, g_history[i]);
    }
    uefi_copy_text(g_history[VITA_CONSOLE_HISTORY_MAX - 1], VITA_CONSOLE_LINE_MAX, line);
}

static void uefi_redraw_input(const char *line, unsigned long len, unsigned long *last_drawn_len) {
    unsigned long i;
    unsigned long old_len;

    old_len = last_drawn_len ? *last_drawn_len : 0;

    uefi_output_raw("\r> ");
    uefi_output_raw(line ? line : "");

    if (old_len > len) {
        for (i = 0; i < old_len - len; ++i) {
            uefi_output_raw(" ");
        }
        for (i = 0; i < old_len - len; ++i) {
            uefi_output_raw("\b");
        }
    }

    if (last_drawn_len) {
        *last_drawn_len = len;
    }
}

static bool uefi_console_read_line(char *out, unsigned long out_cap) {
    unsigned long len = 0;
    unsigned long last_drawn_len = 0;
    unsigned long history_cursor;
    efi_input_key_t key;

    if (!out || out_cap == 0) {
        return false;
    }

    out[0] = '\0';

    if (!g_st || !g_st->con_in || !g_st->con_in->read_key_stroke) {
        return false;
    }

    history_cursor = g_history_count;

    for (;;) {
        key.scan_code = 0;
        key.unicode_char = 0;

        if (!uefi_try_read_key(&key)) {
            uefi_poll_delay();
            continue;
        }

        if (key.unicode_char == 0 && key.scan_code == EFI_SCAN_UP) {
            if (g_history_count > 0 && history_cursor > 0) {
                history_cursor--;
                uefi_copy_text(out, out_cap, g_history[history_cursor]);
                len = uefi_strlen(out);
                uefi_redraw_input(out, len, &last_drawn_len);
            }
            continue;
        }

        if (key.unicode_char == 0 && key.scan_code == EFI_SCAN_DOWN) {
            if (history_cursor < g_history_count) {
                history_cursor++;
                if (history_cursor == g_history_count) {
                    out[0] = '\0';
                } else {
                    uefi_copy_text(out, out_cap, g_history[history_cursor]);
                }
                len = uefi_strlen(out);
                uefi_redraw_input(out, len, &last_drawn_len);
            }
            continue;
        }

        if (key.unicode_char == 13 || key.unicode_char == 10) {
            out[len] = '\0';
            uefi_history_push(out);
            uefi_output_raw("\r\n");
            return true;
        }

        if (key.unicode_char == 8 || key.unicode_char == 127) {
            if (len > 0) {
                len--;
                out[len] = '\0';
                uefi_output_raw("\b \b");
                last_drawn_len = len;
            }
            continue;
        }

        if (key.unicode_char < 128) {
            char c = (char)key.unicode_char;
            if (uefi_allowed_input_char(c) && (len + 1) < out_cap) {
                out[len++] = c;
                out[len] = '\0';
                uefi_echo_char(c);
                last_drawn_len = len;
            }
        }
    }
}

efi_status_t efi_main(efi_handle_t image_handle, efi_system_table_t *system_table) {
    static vita_handoff_t handoff;

    g_st = system_table;
    console_bind_writer(uefi_console_write);
    console_bind_raw_writer(uefi_console_write_raw);
    console_bind_reader(uefi_console_read_line);
    console_bind_clear(uefi_console_clear);

    /*
     * Optional visual boot splash.
     * If /img/1.bmp ... /img/4.bmp are missing or GOP is unavailable,
     * this returns silently and the normal text-first boot continues.
     */
    vita_uefi_show_splash(image_handle, system_table);

    /* Clear firmware/splash framebuffer artifacts before text console. */
    uefi_console_clear();

    handoff.magic = VITA_BOOT_MAGIC;
    handoff.version = 1;
    handoff.size = sizeof(vita_handoff_t);
    handoff.arch_name = "x86_64";
    handoff.firmware_type = VITA_FIRMWARE_UEFI;
    handoff.audit_db_path = 0;
    handoff.vitanet_seed_endpoint = 0;
    handoff.uefi_image_handle = image_handle;
    handoff.uefi_system_table = system_table;

    kmain(&handoff);
    return EFI_SUCCESS;
}
