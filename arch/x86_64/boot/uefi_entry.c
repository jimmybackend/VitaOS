/*
 * arch/x86_64/boot/uefi_entry.c
 * UEFI x86_64 entrypoint with explicit handoff to kmain.
 */

#include <stddef.h>
#include <stdint.h>

#include <vita/boot.h>
#include <vita/console.h>

#define EFI_SUCCESS 0

typedef uint64_t efi_status_t;
typedef void *efi_handle_t;
typedef uint16_t char16_t;

typedef struct efi_simple_text_output_protocol efi_simple_text_output_protocol_t;

typedef struct {
    uint64_t signature;
    uint32_t revision;
    uint32_t header_size;
    uint32_t crc32;
    uint32_t reserved;
} efi_table_header_t;

typedef struct {
    efi_table_header_t hdr;
    char16_t *firmware_vendor;
    uint32_t firmware_revision;
    uint32_t _pad0;
    efi_handle_t console_in_handle;
    void *con_in;
    efi_handle_t console_out_handle;
    efi_simple_text_output_protocol_t *con_out;
} efi_system_table_t;

struct efi_simple_text_output_protocol {
    efi_status_t (*reset)(efi_simple_text_output_protocol_t *self, uint8_t extended_verification);
    efi_status_t (*output_string)(efi_simple_text_output_protocol_t *self, char16_t *string);
};

void kmain(const vita_handoff_t *handoff);

static efi_system_table_t *g_st = 0;

static void ascii_to_char16(const char *src, char16_t *dst, size_t dst_cap) {
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

    dst[i++] = '\r';
    dst[i++] = '\n';
    dst[i] = 0;
}

static void uefi_console_write(const char *text) {
    char16_t buffer[256];

    if (!g_st || !g_st->con_out || !g_st->con_out->output_string) {
        return;
    }

    ascii_to_char16(text, buffer, sizeof(buffer) / sizeof(buffer[0]));
    g_st->con_out->output_string(g_st->con_out, buffer);
}

efi_status_t efi_main(efi_handle_t image_handle, efi_system_table_t *system_table) {
    static vita_handoff_t handoff;

    g_st = system_table;
    console_bind_writer(uefi_console_write);

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
