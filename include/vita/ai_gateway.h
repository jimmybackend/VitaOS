#ifndef VITA_AI_GATEWAY_H
#define VITA_AI_GATEWAY_H

#include <stdbool.h>

/*
 * Remote assistant gateway stub for the F1A/F1B slice.
 *
 * This module deliberately does not open sockets yet. It keeps endpoint
 * configuration in memory, exposes guided console commands, and provides an
 * auditable local fallback response until the network transport exists.
 */

void ai_gateway_show_help(void);
void ai_gateway_show_status(void);
bool ai_gateway_handle_command(const char *line);

#endif
