/*
 * arch/x86_64/boot/uefi_neon_text.c
 * First GOP framebuffer bitmap font renderer for VitaOS F1A/F1B.
 *
 * Scope:
 * - Draws a centered neon terminal shell with a tiny built-in 5x7 font.
 * - Routes console text into the framebuffer when GOP is available.
 * - Keeps a small in-memory scrollback viewport for PageUp/PageDown.
 * - Falls back silently to the existing UEFI text console when unavailable.
 * - No malloc, no external font files, no GUI/window manager.
 */

#include <stdint.h>

#include "uefi_neon_text.h"

#define EFI_SUCCESS 0ULL
#define NEON_SCROLLBACK_LINES 96U
#define NEON_LINE_MAX 132U

typedef uint16_t char16_t;
typedef uint64_t efi_status_t;
typedef void *efi_handle_t;

typedef struct {
    uint32_t data1;
    uint16_t data2;
    uint16_t data3;
    uint8_t data4[8];
} efi_guid_t;

typedef struct {
    uint64_t signature;
    uint32_t revision;
    uint32_t header_size;
    uint32_t crc32;
    uint32_t reserved;
} efi_table_header_t;

typedef struct efi_boot_services efi_boot_services_t;
typedef struct efi_system_table efi_system_table_t;
typedef struct efi_graphics_output_protocol efi_graphics_output_protocol_t;

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
    void *wait_for_event;
    void *signal_event;
    void *close_event;
    void *check_event;

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
    void *stall;
    void *set_watchdog_timer;
    void *connect_controller;
    void *disconnect_controller;
    void *open_protocol;
    void *close_protocol;
    void *open_protocol_information;
    void *protocols_per_handle;
    void *locate_handle_buffer;

    efi_status_t (*locate_protocol)(efi_guid_t *protocol,
                                    void *registration,
                                    void **interface);

    void *install_multiple_protocol_interfaces;
    void *uninstall_multiple_protocol_interfaces;
    void *calculate_crc32;
    void *copy_mem;
    void *set_mem;
    void *create_event_ex;
};

struct efi_system_table {
    efi_table_header_t hdr;
    char16_t *firmware_vendor;
    uint32_t firmware_revision;
    uint32_t _pad0;

    efi_handle_t console_in_handle;
    void *con_in;

    efi_handle_t console_out_handle;
    void *con_out;

    efi_handle_t standard_error_handle;
    void *std_err;

    void *runtime_services;
    efi_boot_services_t *boot_services;

    uint64_t number_of_table_entries;
    void *configuration_table;
};

typedef struct {
    uint32_t version;
    uint32_t horizontal_resolution;
    uint32_t vertical_resolution;
    uint32_t pixel_format;
    uint32_t pixel_information[4];
    uint32_t pixels_per_scan_line;
} efi_graphics_output_mode_information_t;

typedef struct {
    uint32_t max_mode;
    uint32_t mode;
    efi_graphics_output_mode_information_t *info;
    uint64_t size_of_info;
    uint64_t frame_buffer_base;
    uint64_t frame_buffer_size;
} efi_graphics_output_protocol_mode_t;

struct efi_graphics_output_protocol {
    void *query_mode;
    void *set_mode;
    void *blt;
    efi_graphics_output_protocol_mode_t *mode;
};

static efi_guid_t g_graphics_output_protocol_guid = {
    0x9042a9de, 0x23dc, 0x4a38,
    {0x96, 0xfb, 0x7a, 0xde, 0xd0, 0x80, 0x51, 0x6a}
};

typedef struct {
    int ready;
    efi_graphics_output_protocol_t *gop;
    uint32_t screen_w;
    uint32_t screen_h;
    uint32_t stride;
    uint32_t panel_x;
    uint32_t panel_y;
    uint32_t panel_w;
    uint32_t panel_h;
    uint32_t body_x;
    uint32_t body_y;
    uint32_t body_w;
    uint32_t body_h;
    uint32_t cursor_x;
    uint32_t cursor_y;
    uint32_t char_w;
    uint32_t char_h;
    uint32_t scale;
    uint32_t bg;
    uint32_t panel_bg;
    uint32_t title_bg;
    uint32_t border;
    uint32_t text;
    uint32_t dim;
    uint32_t warning;

    uint32_t visible_rows;
    uint32_t columns;
    uint32_t line_start;
    uint32_t line_count;
    uint32_t current_len;
    uint32_t view_offset;
    char lines[NEON_SCROLLBACK_LINES][NEON_LINE_MAX];
    char current_line[NEON_LINE_MAX];
} neon_text_state_t;

static neon_text_state_t g_neon;

static uint32_t make_pixel(uint8_t r, uint8_t g, uint8_t b) {
    uint32_t fmt;

    if (!g_neon.gop || !g_neon.gop->mode || !g_neon.gop->mode->info) {
        return 0;
    }

    fmt = g_neon.gop->mode->info->pixel_format;

    /* 0 = RGBx, 1 = BGRx. Most OVMF/hardware uses BGRx. */
    if (fmt == 0U) {
        return ((uint32_t)r) | ((uint32_t)g << 8) | ((uint32_t)b << 16);
    }

    return ((uint32_t)b) | ((uint32_t)g << 8) | ((uint32_t)r << 16);
}

static void put_pixel(uint32_t x, uint32_t y, uint32_t color) {
    volatile uint32_t *fb;

    if (!g_neon.ready || !g_neon.gop || !g_neon.gop->mode || !g_neon.gop->mode->frame_buffer_base) {
        return;
    }

    if (x >= g_neon.screen_w || y >= g_neon.screen_h) {
        return;
    }

    fb = (volatile uint32_t *)(uintptr_t)g_neon.gop->mode->frame_buffer_base;
    fb[(y * g_neon.stride) + x] = color;
}

static void fill_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color) {
    uint32_t yy;
    uint32_t xx;
    uint32_t max_x = x + w;
    uint32_t max_y = y + h;

    if (max_x > g_neon.screen_w) {
        max_x = g_neon.screen_w;
    }
    if (max_y > g_neon.screen_h) {
        max_y = g_neon.screen_h;
    }

    for (yy = y; yy < max_y; ++yy) {
        for (xx = x; xx < max_x; ++xx) {
            put_pixel(xx, yy, color);
        }
    }
}

static void draw_hline(uint32_t x, uint32_t y, uint32_t w, uint32_t color) {
    fill_rect(x, y, w, 2U, color);
}

static void draw_vline(uint32_t x, uint32_t y, uint32_t h, uint32_t color) {
    fill_rect(x, y, 2U, h, color);
}

static void draw_box(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t border, uint32_t bg) {
    fill_rect(x, y, w, h, bg);
    draw_hline(x, y, w, border);
    draw_hline(x, y + h - 2U, w, border);
    draw_vline(x, y, h, border);
    draw_vline(x + w - 2U, y, h, border);
}

static void glyph5x7(char c, uint8_t rows[7]) {
    int i;

    for (i = 0; i < 7; ++i) {
        rows[i] = 0U;
    }

    if (c >= 'a' && c <= 'z') {
        c = (char)(c - 'a' + 'A');
    }

    switch (c) {
        case 'A': rows[0]=0x0E; rows[1]=0x11; rows[2]=0x11; rows[3]=0x1F; rows[4]=0x11; rows[5]=0x11; rows[6]=0x11; break;
        case 'B': rows[0]=0x1E; rows[1]=0x11; rows[2]=0x11; rows[3]=0x1E; rows[4]=0x11; rows[5]=0x11; rows[6]=0x1E; break;
        case 'C': rows[0]=0x0E; rows[1]=0x11; rows[2]=0x10; rows[3]=0x10; rows[4]=0x10; rows[5]=0x11; rows[6]=0x0E; break;
        case 'D': rows[0]=0x1E; rows[1]=0x11; rows[2]=0x11; rows[3]=0x11; rows[4]=0x11; rows[5]=0x11; rows[6]=0x1E; break;
        case 'E': rows[0]=0x1F; rows[1]=0x10; rows[2]=0x10; rows[3]=0x1E; rows[4]=0x10; rows[5]=0x10; rows[6]=0x1F; break;
        case 'F': rows[0]=0x1F; rows[1]=0x10; rows[2]=0x10; rows[3]=0x1E; rows[4]=0x10; rows[5]=0x10; rows[6]=0x10; break;
        case 'G': rows[0]=0x0E; rows[1]=0x11; rows[2]=0x10; rows[3]=0x17; rows[4]=0x11; rows[5]=0x11; rows[6]=0x0F; break;
        case 'H': rows[0]=0x11; rows[1]=0x11; rows[2]=0x11; rows[3]=0x1F; rows[4]=0x11; rows[5]=0x11; rows[6]=0x11; break;
        case 'I': rows[0]=0x1F; rows[1]=0x04; rows[2]=0x04; rows[3]=0x04; rows[4]=0x04; rows[5]=0x04; rows[6]=0x1F; break;
        case 'J': rows[0]=0x07; rows[1]=0x02; rows[2]=0x02; rows[3]=0x02; rows[4]=0x12; rows[5]=0x12; rows[6]=0x0C; break;
        case 'K': rows[0]=0x11; rows[1]=0x12; rows[2]=0x14; rows[3]=0x18; rows[4]=0x14; rows[5]=0x12; rows[6]=0x11; break;
        case 'L': rows[0]=0x10; rows[1]=0x10; rows[2]=0x10; rows[3]=0x10; rows[4]=0x10; rows[5]=0x10; rows[6]=0x1F; break;
        case 'M': rows[0]=0x11; rows[1]=0x1B; rows[2]=0x15; rows[3]=0x15; rows[4]=0x11; rows[5]=0x11; rows[6]=0x11; break;
        case 'N': rows[0]=0x11; rows[1]=0x19; rows[2]=0x15; rows[3]=0x13; rows[4]=0x11; rows[5]=0x11; rows[6]=0x11; break;
        case 'O': rows[0]=0x0E; rows[1]=0x11; rows[2]=0x11; rows[3]=0x11; rows[4]=0x11; rows[5]=0x11; rows[6]=0x0E; break;
        case 'P': rows[0]=0x1E; rows[1]=0x11; rows[2]=0x11; rows[3]=0x1E; rows[4]=0x10; rows[5]=0x10; rows[6]=0x10; break;
        case 'Q': rows[0]=0x0E; rows[1]=0x11; rows[2]=0x11; rows[3]=0x11; rows[4]=0x15; rows[5]=0x12; rows[6]=0x0D; break;
        case 'R': rows[0]=0x1E; rows[1]=0x11; rows[2]=0x11; rows[3]=0x1E; rows[4]=0x14; rows[5]=0x12; rows[6]=0x11; break;
        case 'S': rows[0]=0x0F; rows[1]=0x10; rows[2]=0x10; rows[3]=0x0E; rows[4]=0x01; rows[5]=0x01; rows[6]=0x1E; break;
        case 'T': rows[0]=0x1F; rows[1]=0x04; rows[2]=0x04; rows[3]=0x04; rows[4]=0x04; rows[5]=0x04; rows[6]=0x04; break;
        case 'U': rows[0]=0x11; rows[1]=0x11; rows[2]=0x11; rows[3]=0x11; rows[4]=0x11; rows[5]=0x11; rows[6]=0x0E; break;
        case 'V': rows[0]=0x11; rows[1]=0x11; rows[2]=0x11; rows[3]=0x11; rows[4]=0x11; rows[5]=0x0A; rows[6]=0x04; break;
        case 'W': rows[0]=0x11; rows[1]=0x11; rows[2]=0x11; rows[3]=0x15; rows[4]=0x15; rows[5]=0x15; rows[6]=0x0A; break;
        case 'X': rows[0]=0x11; rows[1]=0x11; rows[2]=0x0A; rows[3]=0x04; rows[4]=0x0A; rows[5]=0x11; rows[6]=0x11; break;
        case 'Y': rows[0]=0x11; rows[1]=0x11; rows[2]=0x0A; rows[3]=0x04; rows[4]=0x04; rows[5]=0x04; rows[6]=0x04; break;
        case 'Z': rows[0]=0x1F; rows[1]=0x01; rows[2]=0x02; rows[3]=0x04; rows[4]=0x08; rows[5]=0x10; rows[6]=0x1F; break;
        case '0': rows[0]=0x0E; rows[1]=0x11; rows[2]=0x13; rows[3]=0x15; rows[4]=0x19; rows[5]=0x11; rows[6]=0x0E; break;
        case '1': rows[0]=0x04; rows[1]=0x0C; rows[2]=0x04; rows[3]=0x04; rows[4]=0x04; rows[5]=0x04; rows[6]=0x0E; break;
        case '2': rows[0]=0x0E; rows[1]=0x11; rows[2]=0x01; rows[3]=0x02; rows[4]=0x04; rows[5]=0x08; rows[6]=0x1F; break;
        case '3': rows[0]=0x1E; rows[1]=0x01; rows[2]=0x01; rows[3]=0x0E; rows[4]=0x01; rows[5]=0x01; rows[6]=0x1E; break;
        case '4': rows[0]=0x02; rows[1]=0x06; rows[2]=0x0A; rows[3]=0x12; rows[4]=0x1F; rows[5]=0x02; rows[6]=0x02; break;
        case '5': rows[0]=0x1F; rows[1]=0x10; rows[2]=0x10; rows[3]=0x1E; rows[4]=0x01; rows[5]=0x01; rows[6]=0x1E; break;
        case '6': rows[0]=0x0E; rows[1]=0x10; rows[2]=0x10; rows[3]=0x1E; rows[4]=0x11; rows[5]=0x11; rows[6]=0x0E; break;
        case '7': rows[0]=0x1F; rows[1]=0x01; rows[2]=0x02; rows[3]=0x04; rows[4]=0x08; rows[5]=0x08; rows[6]=0x08; break;
        case '8': rows[0]=0x0E; rows[1]=0x11; rows[2]=0x11; rows[3]=0x0E; rows[4]=0x11; rows[5]=0x11; rows[6]=0x0E; break;
        case '9': rows[0]=0x0E; rows[1]=0x11; rows[2]=0x11; rows[3]=0x0F; rows[4]=0x01; rows[5]=0x01; rows[6]=0x0E; break;
        case ':': rows[1]=0x04; rows[2]=0x04; rows[4]=0x04; rows[5]=0x04; break;
        case '.': rows[5]=0x0C; rows[6]=0x0C; break;
        case ',': rows[5]=0x04; rows[6]=0x08; break;
        case '-': rows[3]=0x1F; break;
        case '_': rows[6]=0x1F; break;
        case '/': rows[0]=0x01; rows[1]=0x02; rows[2]=0x02; rows[3]=0x04; rows[4]=0x08; rows[5]=0x08; rows[6]=0x10; break;
        case '\\': rows[0]=0x10; rows[1]=0x08; rows[2]=0x08; rows[3]=0x04; rows[4]=0x02; rows[5]=0x02; rows[6]=0x01; break;
        case '>': rows[1]=0x10; rows[2]=0x08; rows[3]=0x04; rows[4]=0x08; rows[5]=0x10; break;
        case '<': rows[1]=0x01; rows[2]=0x02; rows[3]=0x04; rows[4]=0x02; rows[5]=0x01; break;
        case '[': rows[0]=0x0E; rows[1]=0x08; rows[2]=0x08; rows[3]=0x08; rows[4]=0x08; rows[5]=0x08; rows[6]=0x0E; break;
        case ']': rows[0]=0x0E; rows[1]=0x02; rows[2]=0x02; rows[3]=0x02; rows[4]=0x02; rows[5]=0x02; rows[6]=0x0E; break;
        case '(': rows[0]=0x02; rows[1]=0x04; rows[2]=0x08; rows[3]=0x08; rows[4]=0x08; rows[5]=0x04; rows[6]=0x02; break;
        case ')': rows[0]=0x08; rows[1]=0x04; rows[2]=0x02; rows[3]=0x02; rows[4]=0x02; rows[5]=0x04; rows[6]=0x08; break;
        case '!': rows[0]=0x04; rows[1]=0x04; rows[2]=0x04; rows[3]=0x04; rows[5]=0x04; break;
        case '?': rows[0]=0x0E; rows[1]=0x11; rows[2]=0x01; rows[3]=0x02; rows[4]=0x04; rows[6]=0x04; break;
        case '+': rows[1]=0x04; rows[2]=0x04; rows[3]=0x1F; rows[4]=0x04; rows[5]=0x04; break;
        case '=': rows[2]=0x1F; rows[4]=0x1F; break;
        case '*': rows[1]=0x15; rows[2]=0x0E; rows[3]=0x1F; rows[4]=0x0E; rows[5]=0x15; break;
        case ' ': default: break;
    }
}

static void draw_char_at(uint32_t x, uint32_t y, char c, uint32_t color, uint32_t bg) {
    uint8_t rows[7];
    uint32_t row;
    uint32_t col;
    uint32_t scale = g_neon.scale;

    fill_rect(x, y, g_neon.char_w, g_neon.char_h, bg);

    if (c == ' ') {
        return;
    }

    glyph5x7(c, rows);

    for (row = 0; row < 7U; ++row) {
        for (col = 0; col < 5U; ++col) {
            if ((rows[row] & (uint8_t)(1U << (4U - col))) != 0U) {
                fill_rect(x + 1U + (col * scale),
                          y + 1U + (row * scale),
                          scale,
                          scale,
                          color);
            }
        }
    }
}

static void draw_text_at(uint32_t x, uint32_t y, const char *text, uint32_t color, uint32_t bg) {
    uint32_t cx = x;
    unsigned long i = 0;

    if (!text) {
        return;
    }

    while (text[i]) {
        draw_char_at(cx, y, text[i], color, bg);
        cx += g_neon.char_w;
        i++;
    }
}

static uint32_t str_px_width(const char *s) {
    uint32_t n = 0;

    if (!s) {
        return 0;
    }

    while (s[n]) {
        n++;
    }

    return n * g_neon.char_w;
}

static void clear_body_area_only(void) {
    fill_rect(g_neon.body_x,
              g_neon.body_y,
              g_neon.body_w,
              g_neon.body_h,
              g_neon.panel_bg);
}

static void zero_line(char *line) {
    unsigned long i;

    if (!line) {
        return;
    }

    for (i = 0; i < NEON_LINE_MAX; ++i) {
        line[i] = '\0';
    }
}

static void reset_scrollback(void) {
    uint32_t i;

    g_neon.line_start = 0U;
    g_neon.line_count = 0U;
    g_neon.current_len = 0U;
    g_neon.view_offset = 0U;

    for (i = 0U; i < NEON_SCROLLBACK_LINES; ++i) {
        zero_line(g_neon.lines[i]);
    }

    zero_line(g_neon.current_line);
}

static void copy_line(char *dst, const char *src) {
    unsigned long i = 0;

    if (!dst) {
        return;
    }

    if (!src) {
        dst[0] = '\0';
        return;
    }

    while (src[i] && (i + 1U) < NEON_LINE_MAX) {
        dst[i] = src[i];
        i++;
    }

    dst[i] = '\0';
}

static void push_completed_line(const char *line) {
    uint32_t slot;

    if (g_neon.line_count < NEON_SCROLLBACK_LINES) {
        slot = (g_neon.line_start + g_neon.line_count) % NEON_SCROLLBACK_LINES;
        g_neon.line_count++;
    } else {
        slot = g_neon.line_start;
        g_neon.line_start = (g_neon.line_start + 1U) % NEON_SCROLLBACK_LINES;
    }

    copy_line(g_neon.lines[slot], line ? line : "");
}

static const char *line_at(uint32_t index) {
    uint32_t slot;

    if (index < g_neon.line_count) {
        slot = (g_neon.line_start + index) % NEON_SCROLLBACK_LINES;
        return g_neon.lines[slot];
    }

    return g_neon.current_line;
}

static uint32_t total_visible_source_lines(void) {
    return g_neon.line_count + 1U;
}

static uint32_t max_scroll_offset(void) {
    uint32_t total = total_visible_source_lines();

    if (total <= g_neon.visible_rows) {
        return 0U;
    }

    return total - g_neon.visible_rows;
}

static void clamp_scroll_offset(void) {
    uint32_t max_offset = max_scroll_offset();

    if (g_neon.view_offset > max_offset) {
        g_neon.view_offset = max_offset;
    }
}

static void render_body(void) {
    uint32_t row;
    uint32_t total;
    uint32_t end_index;
    uint32_t start_index;
    uint32_t y;

    if (!g_neon.ready) {
        return;
    }

    clear_body_area_only();
    clamp_scroll_offset();

    total = total_visible_source_lines();
    end_index = (total > g_neon.view_offset) ? (total - g_neon.view_offset) : 0U;
    start_index = (end_index > g_neon.visible_rows) ? (end_index - g_neon.visible_rows) : 0U;

    y = g_neon.body_y + 10U;
    for (row = 0U; row < g_neon.visible_rows; ++row) {
        uint32_t src_index = start_index + row;

        if (src_index >= end_index || y + g_neon.char_h >= g_neon.body_y + g_neon.body_h) {
            break;
        }

        draw_text_at(g_neon.body_x + 12U,
                     y,
                     line_at(src_index),
                     (g_neon.view_offset > 0U) ? g_neon.dim : g_neon.text,
                     g_neon.panel_bg);
        y += g_neon.char_h + 2U;
    }

    if (g_neon.view_offset > 0U) {
        draw_text_at(g_neon.body_x + 12U,
                     g_neon.body_y + g_neon.body_h - g_neon.char_h - 8U,
                     "-- scrollback: PageDown to return / PageDown para volver --",
                     g_neon.warning,
                     g_neon.panel_bg);
    }
}

static void newline(void) {
    push_completed_line(g_neon.current_line);
    zero_line(g_neon.current_line);
    g_neon.current_len = 0U;
    g_neon.view_offset = 0U;
}

static void append_char_to_current(char c) {
    if (g_neon.columns == 0U) {
        return;
    }

    if ((c == '\r') || (c == '\0')) {
        return;
    }

    if (c == '\n') {
        newline();
        return;
    }

    if (c == '\b') {
        if (g_neon.current_len > 0U) {
            g_neon.current_len--;
            g_neon.current_line[g_neon.current_len] = '\0';
        }
        g_neon.view_offset = 0U;
        return;
    }

    if (g_neon.current_len + 1U >= NEON_LINE_MAX || g_neon.current_len >= g_neon.columns) {
        newline();
    }

    if (g_neon.current_len + 1U < NEON_LINE_MAX) {
        g_neon.current_line[g_neon.current_len++] = c;
        g_neon.current_line[g_neon.current_len] = '\0';
    }

    g_neon.view_offset = 0U;
}

static int locate_gop(efi_system_table_t *st, efi_graphics_output_protocol_t **out_gop) {
    void *iface = 0;

    if (!st || !st->boot_services || !st->boot_services->locate_protocol || !out_gop) {
        return 0;
    }

    if (st->boot_services->locate_protocol(&g_graphics_output_protocol_guid, 0, &iface) != EFI_SUCCESS) {
        return 0;
    }

    *out_gop = (efi_graphics_output_protocol_t *)iface;
    return *out_gop != 0;
}

static void compute_layout(void) {
    uint32_t margin_x;
    uint32_t margin_y;

    if (g_neon.screen_w >= 1280U) {
        g_neon.panel_w = 1040U;
    } else if (g_neon.screen_w >= 1024U) {
        g_neon.panel_w = 900U;
    } else {
        g_neon.panel_w = (g_neon.screen_w > 80U) ? (g_neon.screen_w - 60U) : g_neon.screen_w;
    }

    if (g_neon.screen_h >= 800U) {
        g_neon.panel_h = 640U;
    } else if (g_neon.screen_h >= 600U) {
        g_neon.panel_h = 500U;
    } else {
        g_neon.panel_h = (g_neon.screen_h > 80U) ? (g_neon.screen_h - 40U) : g_neon.screen_h;
    }

    margin_x = (g_neon.screen_w > g_neon.panel_w) ? ((g_neon.screen_w - g_neon.panel_w) / 2U) : 0U;
    margin_y = (g_neon.screen_h > g_neon.panel_h) ? ((g_neon.screen_h - g_neon.panel_h) / 2U) : 0U;

    g_neon.panel_x = margin_x;
    g_neon.panel_y = margin_y;
    g_neon.body_x = g_neon.panel_x + 12U;
    g_neon.body_y = g_neon.panel_y + 72U;
    g_neon.body_w = (g_neon.panel_w > 24U) ? (g_neon.panel_w - 24U) : g_neon.panel_w;
    g_neon.body_h = (g_neon.panel_h > 92U) ? (g_neon.panel_h - 104U) : g_neon.panel_h;

    g_neon.scale = (g_neon.screen_w >= 1280U && g_neon.screen_h >= 720U) ? 2U : 1U;
    g_neon.char_w = (6U * g_neon.scale) + 2U;
    g_neon.char_h = (8U * g_neon.scale) + 2U;
    g_neon.visible_rows = (g_neon.body_h > 24U) ? ((g_neon.body_h - 20U) / (g_neon.char_h + 2U)) : 1U;
    if (g_neon.visible_rows == 0U) {
        g_neon.visible_rows = 1U;
    }
    g_neon.columns = (g_neon.body_w > 32U) ? ((g_neon.body_w - 24U) / g_neon.char_w) : 1U;
    if (g_neon.columns >= NEON_LINE_MAX) {
        g_neon.columns = NEON_LINE_MAX - 1U;
    }
}

static void draw_frame(void) {
    uint32_t title_h = 42U;
    uint32_t control_w = 92U;
    uint32_t control_h = 24U;
    uint32_t control_y;
    uint32_t power_x;
    uint32_t restart_x;
    uint32_t min_x;
    uint32_t title_y;
    uint32_t subtitle_y;

    fill_rect(0U, 0U, g_neon.screen_w, g_neon.screen_h, g_neon.bg);
    draw_box(g_neon.panel_x, g_neon.panel_y, g_neon.panel_w, g_neon.panel_h, g_neon.border, g_neon.panel_bg);
    fill_rect(g_neon.panel_x + 2U, g_neon.panel_y + 2U, g_neon.panel_w - 4U, title_h, g_neon.title_bg);
    draw_hline(g_neon.panel_x, g_neon.panel_y + title_h + 2U, g_neon.panel_w, g_neon.border);

    title_y = g_neon.panel_y + 14U;
    draw_text_at(g_neon.panel_x + 18U, title_y, "VITAOS TERMINAL", g_neon.text, g_neon.title_bg);
    draw_text_at(g_neon.panel_x + 18U + str_px_width("VITAOS TERMINAL "), title_y, "F1A/F1B", g_neon.dim, g_neon.title_bg);

    control_y = g_neon.panel_y + 10U;
    power_x = g_neon.panel_x + g_neon.panel_w - control_w - 16U;
    restart_x = power_x - control_w - 8U;
    min_x = restart_x - control_w - 8U;

    draw_box(min_x, control_y, control_w, control_h, g_neon.border, g_neon.bg);
    draw_box(restart_x, control_y, control_w, control_h, g_neon.border, g_neon.bg);
    draw_box(power_x, control_y, control_w, control_h, g_neon.warning, g_neon.bg);

    draw_text_at(min_x + 18U, control_y + 7U, "MIN", g_neon.text, g_neon.bg);
    draw_text_at(restart_x + 14U, control_y + 7U, "RESTART", g_neon.text, g_neon.bg);
    draw_text_at(power_x + 18U, control_y + 7U, "POWER", g_neon.warning, g_neon.bg);

    subtitle_y = g_neon.panel_y + 52U;
    draw_text_at(g_neon.panel_x + 18U, subtitle_y, "CENTERED NEON FRAMEBUFFER TERMINAL / TERMINAL NEON CENTRADA", g_neon.dim, g_neon.panel_bg);

    draw_box(g_neon.body_x, g_neon.body_y, g_neon.body_w, g_neon.body_h, g_neon.border, g_neon.panel_bg);
    draw_hline(g_neon.body_x, g_neon.panel_y + g_neon.panel_h - 34U, g_neon.body_w, g_neon.border);
    draw_text_at(g_neon.body_x + 10U, g_neon.panel_y + g_neon.panel_h - 24U, "PROMPT >", g_neon.dim, g_neon.panel_bg);

    render_body();
}

void vita_uefi_neon_text_init(void *image_handle, void *system_table) {
    efi_system_table_t *st = (efi_system_table_t *)system_table;
    efi_graphics_output_protocol_t *gop = 0;

    (void)image_handle;

    g_neon.ready = 0;
    g_neon.gop = 0;

    if (!locate_gop(st, &gop)) {
        return;
    }

    if (!gop || !gop->mode || !gop->mode->info || !gop->mode->frame_buffer_base) {
        return;
    }

    g_neon.gop = gop;
    g_neon.screen_w = gop->mode->info->horizontal_resolution;
    g_neon.screen_h = gop->mode->info->vertical_resolution;
    g_neon.stride = gop->mode->info->pixels_per_scan_line;

    if (g_neon.screen_w < 320U || g_neon.screen_h < 240U) {
        g_neon.ready = 0;
        return;
    }

    g_neon.bg = make_pixel(0U, 3U, 10U);
    g_neon.panel_bg = make_pixel(0U, 10U, 22U);
    g_neon.title_bg = make_pixel(0U, 28U, 44U);
    g_neon.border = make_pixel(0U, 210U, 255U);
    g_neon.text = make_pixel(120U, 240U, 255U);
    g_neon.dim = make_pixel(30U, 125U, 160U);
    g_neon.warning = make_pixel(255U, 90U, 80U);

    compute_layout();
    reset_scrollback();
    g_neon.ready = 1;
    draw_frame();
}

int vita_uefi_neon_text_ready(void) {
    return g_neon.ready;
}

void vita_uefi_neon_text_write_raw(const char *text) {
    unsigned long i = 0;

    if (!g_neon.ready || !text) {
        return;
    }

    while (text[i]) {
        append_char_to_current(text[i]);
        i++;
    }

    render_body();
}

void vita_uefi_neon_text_write_line(const char *text) {
    if (!g_neon.ready) {
        return;
    }

    vita_uefi_neon_text_write_raw(text ? text : "");
    vita_uefi_neon_text_write_raw("\n");
}

void vita_uefi_neon_text_clear(void) {
    if (!g_neon.ready) {
        return;
    }

    reset_scrollback();
    draw_frame();
}

void vita_uefi_neon_text_scroll_up(void) {
    uint32_t step;
    uint32_t max_offset;

    if (!g_neon.ready) {
        return;
    }

    step = (g_neon.visible_rows > 2U) ? (g_neon.visible_rows - 2U) : 1U;
    max_offset = max_scroll_offset();

    if (g_neon.view_offset + step > max_offset) {
        g_neon.view_offset = max_offset;
    } else {
        g_neon.view_offset += step;
    }

    render_body();
}

void vita_uefi_neon_text_scroll_down(void) {
    uint32_t step;

    if (!g_neon.ready) {
        return;
    }

    step = (g_neon.visible_rows > 2U) ? (g_neon.visible_rows - 2U) : 1U;

    if (g_neon.view_offset <= step) {
        g_neon.view_offset = 0U;
    } else {
        g_neon.view_offset -= step;
    }

    render_body();
}

void vita_uefi_neon_text_scroll_bottom(void) {
    if (!g_neon.ready) {
        return;
    }

    g_neon.view_offset = 0U;
    render_body();
}
