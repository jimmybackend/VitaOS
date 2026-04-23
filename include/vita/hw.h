#ifndef VITA_HW_H
#define VITA_HW_H

#include <stdbool.h>
#include <stdint.h>

#include <vita/boot.h>

typedef struct {
    char cpu_arch[32];
    char cpu_model[128];
    uint64_t ram_bytes;
    char firmware_type[32];
    char console_type[32];
    int net_count;
    int storage_count;
    int usb_count;
    int wifi_count;
    uint64_t detected_at_unix;
} vita_hw_snapshot_t;

bool hw_discovery_run(const vita_handoff_t *handoff, vita_hw_snapshot_t *out_snapshot);

#endif
