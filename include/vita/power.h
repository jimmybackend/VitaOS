#ifndef VITA_POWER_H
#define VITA_POWER_H

#include <stdbool.h>

#include <vita/command.h>

typedef enum {
    VITA_POWER_ACTION_NONE = 0,
    VITA_POWER_ACTION_SHUTDOWN,
    VITA_POWER_ACTION_RESTART
} vita_power_action_t;

void power_show_status(const vita_command_context_t *ctx);
bool power_request(vita_power_action_t action, const vita_command_context_t *ctx);
bool power_handle_command(const char *cmd, const vita_command_context_t *ctx);

#endif
