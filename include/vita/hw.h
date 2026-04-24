#ifndef VITA_HW_H
#define VITA_HW_H

#include <stdbool.h>
#include <stdint.h>

#include <vita/boot.h>

/*
 * Keep this structure synchronized with:
 * - schema/audit.sql: hardware_snapshot
 * - kernel/audit_core.c: audit_persist_hardware_snapshot()
 */
typedef struct {
    char cpu_arch[32];
    char cpu_model[128];
    uint64_t ram_bytes;

    char firmware_type[32];
    char console_type[32];

    int display_count;
    int keyboard_count;
    int mouse_count;
    int audio_count;
    int microphone_count;

    int net_count;
    int ethernet_count;
    int wifi_count;

    int storage_count;
    int usb_count;
    int usb_controller_count;

    uint64_t detected_at_unix;
} vita_hw_snapshot_t;

bool hw_discovery_run(const vita_handoff_t *handoff, vita_hw_snapshot_t *out_snapshot);

#endif
