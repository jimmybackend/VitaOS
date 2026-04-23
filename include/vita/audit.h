#ifndef VITA_AUDIT_H
#define VITA_AUDIT_H

#include <stdbool.h>

#include <vita/boot.h>
#include <vita/hw.h>
#include <vita/proposal.h>

void audit_early_buffer_init(void);
bool audit_init_persistent_backend(const vita_handoff_t *handoff);
void audit_emit_boot_event(const char *event_type, const char *summary);
bool audit_persist_hardware_snapshot(const vita_hw_snapshot_t *snapshot);
bool audit_persist_ai_proposal(const vita_ai_proposal_t *proposal);
bool audit_persist_human_response(const char *proposal_id, const char *response);

#endif
