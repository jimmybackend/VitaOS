#ifndef VITA_NODE_H
#define VITA_NODE_H

#include <stdbool.h>
#include <stdint.h>

#include <vita/boot.h>

#define VITA_NODE_ID_MAX 64
#define VITA_NODE_TRANSPORT_MAX 32
#define VITA_NODE_STATE_MAX 32
#define VITA_NODE_CAPABILITIES_JSON_MAX 256
#define VITA_NODE_TASK_TYPE_MAX 64
#define VITA_NODE_TASK_STATUS_MAX 32
#define VITA_NODE_TASK_PAYLOAD_JSON_MAX 512

typedef struct {
    char peer_id[VITA_NODE_ID_MAX];
    uint64_t first_seen_unix;
    uint64_t last_seen_unix;
    char transport[VITA_NODE_TRANSPORT_MAX];
    char capabilities_json[VITA_NODE_CAPABILITIES_JSON_MAX];
    char trust_state[VITA_NODE_STATE_MAX];
    char link_state[VITA_NODE_STATE_MAX];
} vita_node_peer_t;

typedef struct {
    char task_id[VITA_NODE_ID_MAX];
    char origin_node_id[VITA_NODE_ID_MAX];
    char target_node_id[VITA_NODE_ID_MAX];
    char task_type[VITA_NODE_TASK_TYPE_MAX];
    char status[VITA_NODE_TASK_STATUS_MAX];
    uint64_t created_unix;
    uint64_t updated_unix;
    char payload_json[VITA_NODE_TASK_PAYLOAD_JSON_MAX];
} vita_node_task_t;

bool node_core_start(const vita_handoff_t *handoff);
int node_core_peer_count(void);
void node_core_show_peers(void);
void node_core_show_link_proposal(void);
int node_core_pending_proposal_count(void);
bool node_core_handle_link_response(const char *proposal_id, bool approve);

#endif
