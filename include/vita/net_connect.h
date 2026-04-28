#ifndef VITA_NET_CONNECT_H
#define VITA_NET_CONNECT_H

#include <stdbool.h>
#include <stdint.h>

#include <vita/boot.h>
#include <vita/hw.h>

typedef enum {
    VITA_NET_CONNECT_DISCONNECTED = 0,
    VITA_NET_CONNECT_CONFIGURED = 1,
    VITA_NET_CONNECT_CONNECTED = 2,
    VITA_NET_CONNECT_LIMITED = 3
} vita_net_connect_state_t;

void net_connect_reset(void);

bool net_connect_configure_endpoint(const char *endpoint_text);
bool net_connect_attempt(void);
void net_connect_disconnect(void);

bool net_connect_send_line(const char *line);

bool net_connect_is_configured(void);
bool net_connect_is_connected(void);
const char *net_connect_host(void);
uint16_t net_connect_port(void);
const char *net_connect_state_name(void);
const char *net_connect_last_error(void);

void net_connect_show_status(const vita_handoff_t *handoff,
                             const vita_hw_snapshot_t *hw,
                             bool hw_ready);

#endif
