/*
 * arch/x86_64/boot/uefi_splash.c
 * Minimal UEFI boot splash for VitaOS F1A/F1B.
 *
 * Scope:
 * - Uses UEFI GOP framebuffer directly.
 * - Opens splash frames from the same EFI filesystem:
 *     /img/1.bmp
 *     /img/2.bmp
 *     /img/3.bmp
 *     /img/4.bmp
 * - Supports BMP 1-bit monochrome, 24-bit RGB, and 32-bit RGB.
 * - Fails silently if GOP, filesystem, or frames are unavailable.
 *
 * This deliberately avoids PNG decoding and complex graphics features.
 */

#include <stddef.h>
#include <stdint.h>

#include "uefi_splash.h"

#define EFI_SUCCESS 0
#define EFI_FILE_MODE_READ 0x0000000000000001ULL

#define BMP_BUFFER_SIZE (1024U * 1024U)
#define SPLASH_FRAME_DELAY_US 180000U

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
typedef struct efi_loaded_image_protocol efi_loaded_image_protocol_t;
typedef struct efi_simple_file_system_protocol efi_simple_file_system_protocol_t;
typedef struct efi_file_protocol efi_file_protocol_t;
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

    efi_status_t (*handle_protocol)(efi_handle_t handle,
                                    efi_guid_t *protocol,
                                    void **interface);

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

struct efi_loaded_image_protocol {
    uint32_t revision;
    efi_handle_t parent_handle;
    efi_system_table_t *system_table;
    efi_handle_t device_handle;
    void *file_path;
    void *reserved;

    uint32_t load_options_size;
    void *load_options;

    void *image_base;
    uint64_t image_size;
    uint32_t image_code_type;
    uint32_t image_data_type;

    efi_status_t (*unload)(efi_handle_t image_handle);
};

struct efi_simple_file_system_protocol {
    uint64_t revision;
    efi_status_t (*open_volume)(efi_simple_file_system_protocol_t *self,
                                efi_file_protocol_t **root);
};

struct efi_file_protocol {
    uint64_t revision;

    efi_status_t (*open)(efi_file_protocol_t *self,
                         efi_file_protocol_t **new_handle,
                         char16_t *file_name,
                         uint64_t open_mode,
                         uint64_t attributes);

    efi_status_t (*close)(efi_file_protocol_t *self);
    efi_status_t (*delete_file)(efi_file_protocol_t *self);

    efi_status_t (*read)(efi_file_protocol_t *self,
                         uint64_t *buffer_size,
                         void *buffer);

    efi_status_t (*write)(efi_file_protocol_t *self,
                          uint64_t *buffer_size,
                          void *buffer);

    efi_status_t (*get_position)(efi_file_protocol_t *self,
                                 uint64_t *position);

    efi_status_t (*set_position)(efi_file_protocol_t *self,
                                 uint64_t position);

    efi_status_t (*get_info)(efi_file_protocol_t *self,
                             efi_guid_t *information_type,
                             uint64_t *buffer_size,
                             void *buffer);

    efi_status_t (*set_info)(efi_file_protocol_t *self,
                             efi_guid_t *information_type,
                             uint64_t buffer_size,
                             void *buffer);

    efi_status_t (*flush)(efi_file_protocol_t *self);
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

static efi_guid_t g_loaded_image_protocol_guid = {
    0x5b1b31a1, 0x9562, 0x11d2,
    {0x8e, 0x3f, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b}
};

static efi_guid_t g_simple_file_system_protocol_guid = {
    0x0964e5b22, 0x6459, 0x11d2,
    {0x8e, 0x39, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b}
};

static efi_guid_t g_graphics_output_protocol_guid = {
    0x9042a9de, 0x23dc, 0x4a38,
    {0x96, 0xfb, 0x7a, 0xde, 0xd0, 0x80, 0x51, 0x6a}
};

static const char16_t g_frame_paths[4][16] = {
    {'\\','i','m','g','\\','1','.','b','m','p',0},
    {'\\','i','m','g','\\','2','.','b','m','p',0},
    {'\\','i','m','g','\\','3','.','b','m','p',0},
    {'\\','i','m','g','\\','4','.','b','m','p',0}
};

static uint8_t g_bmp_buffer[BMP_BUFFER_SIZE];

static uint16_t read_le16(const uint8_t *p) {
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t read_le32(const uint8_t *p) {
    return ((uint32_t)p[0]) |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static int32_t read_le32s(const uint8_t *p) {
    return (int32_t)read_le32(p);
}

static int32_t abs_i32(int32_t v) {
    return (v < 0) ? -v : v;
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

    /*
     * UEFI pixel formats:
     * 0 = PixelRedGreenBlueReserved8BitPerColor
     * 1 = PixelBlueGreenRedReserved8BitPerColor
     */
    if (fmt == 0) {
        return ((uint32_t)r) | ((uint32_t)g << 8) | ((uint32_t)b << 16);
    }

    return ((uint32_t)b) | ((uint32_t)g << 8) | ((uint32_t)r << 16);
}

static void put_pixel(efi_graphics_output_protocol_t *gop,
                      int32_t x,
                      int32_t y,
                      uint32_t pixel) {
    volatile uint32_t *fb;
    uint32_t width;
    uint32_t height;
    uint32_t stride;

    if (!gop || !gop->mode || !gop->mode->info || !gop->mode->frame_buffer_base) {
        return;
    }

    if (x < 0 || y < 0) {
        return;
    }

    width = gop->mode->info->horizontal_resolution;
    height = gop->mode->info->vertical_resolution;
    stride = gop->mode->info->pixels_per_scan_line;

    if ((uint32_t)x >= width || (uint32_t)y >= height) {
        return;
    }

    fb = (volatile uint32_t *)(uintptr_t)gop->mode->frame_buffer_base;
    fb[((uint32_t)y * stride) + (uint32_t)x] = pixel;
}

static uint32_t bmp_palette_pixel(const uint8_t *bmp,
                                  uint32_t palette_index,
                                  const efi_graphics_output_protocol_t *gop) {
    const uint8_t *entry;

    entry = bmp + 54U + (palette_index * 4U);

    return make_pixel(gop, entry[2], entry[1], entry[0]);
}

static uint32_t bmp_direct_pixel(const uint8_t *src,
                                 uint16_t bits_per_pixel,
                                 const efi_graphics_output_protocol_t *gop) {
    if (bits_per_pixel == 24) {
        return make_pixel(gop, src[2], src[1], src[0]);
    }

    if (bits_per_pixel == 32) {
        return make_pixel(gop, src[2], src[1], src[0]);
    }

    return 0;
}

static int bmp_supported(uint16_t bits_per_pixel, uint32_t compression) {
    if (compression != 0) {
        return 0;
    }

    return bits_per_pixel == 1 || bits_per_pixel == 24 || bits_per_pixel == 32;
}

static void draw_bmp_centered(efi_graphics_output_protocol_t *gop,
                              const uint8_t *bmp,
                              uint64_t bmp_size) {
    uint32_t pixel_offset;
    int32_t width;
    int32_t raw_height;
    int32_t height;
    uint16_t bits_per_pixel;
    uint32_t compression;
    uint32_t row_stride;
    int32_t dst_x;
    int32_t dst_y;
    int top_down;

    if (!gop || !gop->mode || !gop->mode->info || !bmp || bmp_size < 62U) {
        return;
    }

    if (bmp[0] != 'B' || bmp[1] != 'M') {
        return;
    }

    pixel_offset = read_le32(bmp + 10);
    width = read_le32s(bmp + 18);
    raw_height = read_le32s(bmp + 22);
    bits_per_pixel = read_le16(bmp + 28);
    compression = read_le32(bmp + 30);

    if (width <= 0 || raw_height == 0) {
        return;
    }

    if (!bmp_supported(bits_per_pixel, compression)) {
        return;
    }

    height = abs_i32(raw_height);
    top_down = raw_height < 0;

    if (pixel_offset >= bmp_size) {
        return;
    }

    row_stride = (uint32_t)((((uint32_t)width * bits_per_pixel) + 31U) / 32U) * 4U;
    if ((uint64_t)pixel_offset + ((uint64_t)row_stride * (uint64_t)height) > bmp_size) {
        return;
    }

    dst_x = ((int32_t)gop->mode->info->horizontal_resolution - width) / 2;
    dst_y = ((int32_t)gop->mode->info->vertical_resolution - height) / 2;

    for (int32_t y = 0; y < height; ++y) {
        int32_t src_y;
        const uint8_t *row;

        src_y = top_down ? y : (height - 1 - y);
        row = bmp + pixel_offset + ((uint32_t)src_y * row_stride);

        for (int32_t x = 0; x < width; ++x) {
            uint32_t pixel = 0;

            if (bits_per_pixel == 1) {
                uint8_t src_byte;
                uint8_t bit;
                uint32_t palette_index;

                src_byte = row[(uint32_t)x / 8U];
                bit = (uint8_t)(7U - ((uint32_t)x & 7U));
                palette_index = (uint32_t)((src_byte >> bit) & 1U);
                pixel = bmp_palette_pixel(bmp, palette_index, gop);
            } else {
                uint32_t bytes_per_pixel = (uint32_t)bits_per_pixel / 8U;
                const uint8_t *src = row + ((uint32_t)x * bytes_per_pixel);
                pixel = bmp_direct_pixel(src, bits_per_pixel, gop);
            }

            put_pixel(gop, dst_x + x, dst_y + y, pixel);
        }
    }
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

static int open_boot_root(efi_boot_services_t *bs,
                          efi_handle_t image_handle,
                          efi_file_protocol_t **out_root) {
    void *loaded_iface = 0;
    void *fs_iface = 0;
    efi_loaded_image_protocol_t *loaded;
    efi_simple_file_system_protocol_t *fs;

    if (!bs || !bs->handle_protocol || !image_handle || !out_root) {
        return 0;
    }

    if (bs->handle_protocol(image_handle,
                            &g_loaded_image_protocol_guid,
                            &loaded_iface) != EFI_SUCCESS) {
        return 0;
    }

    loaded = (efi_loaded_image_protocol_t *)loaded_iface;
    if (!loaded || !loaded->device_handle) {
        return 0;
    }

    if (bs->handle_protocol(loaded->device_handle,
                            &g_simple_file_system_protocol_guid,
                            &fs_iface) != EFI_SUCCESS) {
        return 0;
    }

    fs = (efi_simple_file_system_protocol_t *)fs_iface;
    if (!fs || !fs->open_volume) {
        return 0;
    }

    if (fs->open_volume(fs, out_root) != EFI_SUCCESS) {
        return 0;
    }

    return *out_root != 0;
}

static int read_frame(efi_file_protocol_t *root,
                      const char16_t *path,
                      uint8_t *buffer,
                      uint64_t buffer_cap,
                      uint64_t *out_size) {
    efi_file_protocol_t *file = 0;
    uint64_t size;

    if (!root || !root->open || !path || !buffer || !out_size) {
        return 0;
    }

    if (root->open(root,
                   &file,
                   (char16_t *)path,
                   EFI_FILE_MODE_READ,
                   0) != EFI_SUCCESS) {
        return 0;
    }

    if (!file || !file->read) {
        if (file && file->close) {
            file->close(file);
        }
        return 0;
    }

    size = buffer_cap;
    if (file->read(file, &size, buffer) != EFI_SUCCESS) {
        if (file->close) {
            file->close(file);
        }
        return 0;
    }

    if (file->close) {
        file->close(file);
    }

    *out_size = size;
    return size > 0;
}

void vita_uefi_show_splash(void *image_handle, void *system_table) {
    efi_system_table_t *st = (efi_system_table_t *)system_table;
    efi_boot_services_t *bs;
    efi_graphics_output_protocol_t *gop = 0;
    efi_file_protocol_t *root = 0;

    if (!st || !st->boot_services) {
        return;
    }

    bs = st->boot_services;

    if (!locate_gop(bs, &gop)) {
        return;
    }

    if (!open_boot_root(bs, image_handle, &root)) {
        return;
    }

    for (uint32_t i = 0; i < 4U; ++i) {
        uint64_t frame_size = 0;

        if (read_frame(root,
                       g_frame_paths[i],
                       g_bmp_buffer,
                       sizeof(g_bmp_buffer),
                       &frame_size)) {
            draw_bmp_centered(gop, g_bmp_buffer, frame_size);

            if (bs->stall) {
                bs->stall(SPLASH_FRAME_DELAY_US);
            }
        }
    }

    if (root && root->close) {
        root->close(root);
    }
}
