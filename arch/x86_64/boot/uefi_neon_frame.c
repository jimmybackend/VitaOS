/*
 * arch/x86_64/boot/uefi_neon_frame.c
 * Minimal GOP-backed neon terminal frame renderer for VitaOS F1A/F1B.
 *
 * This is not a GUI compositor. It draws a dark centered terminal panel behind
 * the existing text console so VitaOS keeps its text-first boot path.
 */

#include <stdint.h>

#include "uefi_neon_frame.h"

#define EFI_SUCCESS 0ULL

typedef uint16_t char16_t;
typedef uint64_t efi_status_t;

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

    void *console_in_handle;
    void *con_in;

    void *console_out_handle;
    void *con_out;

    void *standard_error_handle;
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

static uint32_t clamp_u32(uint32_t v, uint32_t min_v, uint32_t max_v) {
    if (v < min_v) {
        return min_v;
    }
    if (v > max_v) {
        return max_v;
    }
    return v;
}

static uint32_t make_pixel(const efi_graphics_output_protocol_t *gop,
                           uint8_t r,
                           uint8_t g,
                           uint8_t b) {
    uint32_t fmt;

    if (!gop || !gop->mode || !gop->mode->info) {
        return 0;
    }

    fmt = gop->mode->info->pixel_format;

    /* 0 = RGBx, 1 = BGRx. Use BGR as safe fallback for common UEFI GOP. */
    if (fmt == 0U) {
        return ((uint32_t)r) | ((uint32_t)g << 8) | ((uint32_t)b << 16);
    }

    return ((uint32_t)b) | ((uint32_t)g << 8) | ((uint32_t)r << 16);
}

static void draw_rect(efi_graphics_output_protocol_t *gop,
                      uint32_t x,
                      uint32_t y,
                      uint32_t w,
                      uint32_t h,
                      uint32_t pixel) {
    volatile uint32_t *fb;
    uint32_t screen_w;
    uint32_t screen_h;
    uint32_t stride;
    uint32_t yy;

    if (!gop || !gop->mode || !gop->mode->info || !gop->mode->frame_buffer_base) {
        return;
    }

    screen_w = gop->mode->info->horizontal_resolution;
    screen_h = gop->mode->info->vertical_resolution;
    stride = gop->mode->info->pixels_per_scan_line;

    if (x >= screen_w || y >= screen_h) {
        return;
    }

    if (x + w > screen_w) {
        w = screen_w - x;
    }
    if (y + h > screen_h) {
        h = screen_h - y;
    }

    fb = (volatile uint32_t *)(uintptr_t)gop->mode->frame_buffer_base;

    for (yy = 0; yy < h; ++yy) {
        volatile uint32_t *row = fb + ((y + yy) * stride) + x;
        uint32_t xx;
        for (xx = 0; xx < w; ++xx) {
            row[xx] = pixel;
        }
    }
}

static void draw_border(efi_graphics_output_protocol_t *gop,
                        uint32_t x,
                        uint32_t y,
                        uint32_t w,
                        uint32_t h,
                        uint32_t thick,
                        uint32_t pixel) {
    if (w <= (thick * 2U) || h <= (thick * 2U)) {
        return;
    }

    draw_rect(gop, x, y, w, thick, pixel);
    draw_rect(gop, x, y + h - thick, w, thick, pixel);
    draw_rect(gop, x, y, thick, h, pixel);
    draw_rect(gop, x + w - thick, y, thick, h, pixel);
}

static int locate_gop(efi_boot_services_t *bs,
                      efi_graphics_output_protocol_t **out_gop) {
    void *iface = 0;

    if (!bs || !bs->locate_protocol || !out_gop) {
        return 0;
    }

    if (bs->locate_protocol(&g_graphics_output_protocol_guid, 0, &iface) != EFI_SUCCESS) {
        return 0;
    }

    *out_gop = (efi_graphics_output_protocol_t *)iface;
    return *out_gop != 0;
}

void vita_uefi_neon_frame_render(void *system_table, int minimized) {
    efi_system_table_t *st = (efi_system_table_t *)system_table;
    efi_graphics_output_protocol_t *gop = 0;
    uint32_t sw;
    uint32_t sh;
    uint32_t bg;
    uint32_t panel;
    uint32_t panel2;
    uint32_t cyan;
    uint32_t cyan_soft;
    uint32_t blue_glow;
    uint32_t shadow;
    uint32_t pw;
    uint32_t ph;
    uint32_t px;
    uint32_t py;
    uint32_t title_h;
    uint32_t glow;

    if (!st || !st->boot_services) {
        return;
    }

    if (!locate_gop(st->boot_services, &gop)) {
        return;
    }

    if (!gop || !gop->mode || !gop->mode->info || !gop->mode->frame_buffer_base) {
        return;
    }

    sw = gop->mode->info->horizontal_resolution;
    sh = gop->mode->info->vertical_resolution;

    if (sw < 320U || sh < 200U) {
        return;
    }

    bg = make_pixel(gop, 0, 3, 10);
    panel = make_pixel(gop, 3, 15, 28);
    panel2 = make_pixel(gop, 0, 24, 42);
    cyan = make_pixel(gop, 0, 224, 255);
    cyan_soft = make_pixel(gop, 0, 96, 140);
    blue_glow = make_pixel(gop, 0, 48, 128);
    shadow = make_pixel(gop, 0, 0, 0);

    draw_rect(gop, 0, 0, sw, sh, bg);

    if (minimized) {
        pw = clamp_u32((sw * 72U) / 100U, 360U, 960U);
        ph = 72U;
        px = (sw - pw) / 2U;
        py = clamp_u32(sh / 10U, 24U, 96U);
        title_h = ph;
    } else {
        pw = clamp_u32((sw * 82U) / 100U, 560U, 1180U);
        ph = clamp_u32((sh * 74U) / 100U, 360U, 760U);
        px = (sw - pw) / 2U;
        py = (sh - ph) / 2U;
        title_h = clamp_u32(ph / 10U, 40U, 58U);
    }

    /* Soft glow outside the panel. */
    for (glow = 0; glow < 5U; ++glow) {
        if (px > (glow + 1U) * 5U && py > (glow + 1U) * 5U) {
            draw_border(gop,
                        px - ((glow + 1U) * 5U),
                        py - ((glow + 1U) * 5U),
                        pw + ((glow + 1U) * 10U),
                        ph + ((glow + 1U) * 10U),
                        1U,
                        blue_glow);
        }
    }

    draw_rect(gop, px + 12U, py + 12U, pw, ph, shadow);
    draw_rect(gop, px, py, pw, ph, panel);
    draw_rect(gop, px, py, pw, title_h, panel2);

    draw_border(gop, px, py, pw, ph, 2U, cyan);
    if (pw > 8U && ph > 8U) {
        draw_border(gop, px + 4U, py + 4U, pw - 8U, ph - 8U, 1U, cyan_soft);
    }

    if (!minimized && ph > title_h + 32U) {
        uint32_t content_y = py + title_h + 18U;
        uint32_t content_h = ph - title_h - 32U;
        draw_rect(gop, px + 22U, content_y, pw - 44U, content_h, make_pixel(gop, 0, 7, 16));
        draw_border(gop, px + 22U, content_y, pw - 44U, content_h, 1U, cyan_soft);

        /* Prompt rail at the bottom. */
        if (content_h > 80U) {
            draw_rect(gop, px + 34U, py + ph - 62U, pw - 68U, 30U, make_pixel(gop, 0, 18, 32));
            draw_border(gop, px + 34U, py + ph - 62U, pw - 68U, 30U, 1U, cyan);
        }
    }

    /* Title bar control boxes, purely visual; click routing remains text/coordinate based. */
    if (pw > 280U && title_h > 28U) {
        uint32_t by = py + 10U;
        uint32_t bw = 58U;
        uint32_t bh = title_h - 20U;
        uint32_t bx3 = px + pw - 72U;
        uint32_t bx2 = bx3 - 70U;
        uint32_t bx1 = bx2 - 70U;

        draw_rect(gop, bx1, by, bw, bh, make_pixel(gop, 0, 34, 52));
        draw_border(gop, bx1, by, bw, bh, 1U, cyan);

        draw_rect(gop, bx2, by, bw, bh, make_pixel(gop, 28, 22, 0));
        draw_border(gop, bx2, by, bw, bh, 1U, make_pixel(gop, 255, 186, 40));

        draw_rect(gop, bx3, by, bw, bh, make_pixel(gop, 42, 4, 10));
        draw_border(gop, bx3, by, bw, bh, 1U, make_pixel(gop, 255, 60, 90));
    }
}
