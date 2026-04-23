#ifndef VITA_BOOT_H
#define VITA_BOOT_H

#include <stdint.h>

#define VITA_BOOT_MAGIC 0x56495441424F4F54ULL /* "VITABOOT" */

typedef enum {
    VITA_FIRMWARE_UNKNOWN = 0,
    VITA_FIRMWARE_UEFI = 1,
    VITA_FIRMWARE_HOSTED = 2,
} vita_firmware_type_t;

typedef struct {
    uint64_t magic;
    uint32_t version;
    uint32_t size;

    const char *arch_name;
    uint64_t firmware_type;

    const char *audit_db_path;

    void *uefi_image_handle;
    void *uefi_system_table;
} vita_handoff_t;

#endif
