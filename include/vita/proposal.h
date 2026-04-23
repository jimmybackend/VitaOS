#ifndef VITA_PROPOSAL_H
#define VITA_PROPOSAL_H

#include <stdbool.h>

#include <vita/boot.h>
#include <vita/hw.h>

typedef struct {
    const char *proposal_id;
    const char *proposal_type;
    const char *summary;
    const char *rationale;
    const char *risk_level;
    const char *benefit_level;
    bool requires_human_confirmation;
    const char *status;
} vita_ai_proposal_t;

void proposal_engine_reset(void);
void proposal_generate_initial(const vita_handoff_t *handoff, const vita_hw_snapshot_t *hw, bool audit_ready);
void proposal_show_all(void);
bool proposal_handle_command(const char *line);
void proposal_hosted_repl(void);
int proposal_count_all(void);
int proposal_count_pending(void);

#endif
