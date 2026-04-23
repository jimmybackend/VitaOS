#ifndef VITA_AUDIT_H
#define VITA_AUDIT_H

#include <stdbool.h>

#include <vita/boot.h>
#include <vita/hw.h>

void audit_early_buffer_init(void);
bool audit_init_persistent_backend(const vita_handoff_t *handoff);
void audit_emit_boot_event(const char *event_type, const char *summary);
bool audit_persist_hardware_snapshot(const vita_hw_snapshot_t *snapshot);

#endif
