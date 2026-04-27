#ifndef VITA_COMMAND_H
#define VITA_COMMAND_H

#include <stdbool.h>

#include <vita/boot.h>
#include <vita/console.h>
#include <vita/hw.h>

typedef enum {
    VITA_COMMAND_CONTINUE = 0,
    VITA_COMMAND_SHUTDOWN = 1
} vita_command_result_t;

typedef struct {
    const vita_handoff_t *handoff;
    vita_boot_status_t boot_status;
    vita_console_state_t console_state;
    vita_hw_snapshot_t hw_snapshot;
    bool hw_ready;
    bool proposal_ready;
    bool node_ready;
} vita_command_context_t;

void command_context_init(vita_command_context_t *ctx,
                          const vita_handoff_t *handoff,
                          const vita_boot_status_t *boot_status,
                          const vita_hw_snapshot_t *hw_snapshot,
                          bool hw_ready,
                          bool proposal_ready,
                          bool node_ready);

vita_command_result_t command_handle_line(vita_command_context_t *ctx, const char *line);
void command_loop_run(vita_command_context_t *ctx);

#endif
