#ifndef VITA_NODE_H
#define VITA_NODE_H

#include <stdbool.h>
#include <stdint.h>

#include <vita/boot.h>

typedef struct {
    char peer_id[64];
    uint64_t first_seen_unix;
    uint64_t last_seen_unix;
    char transport[32];
    char capabilities_json[256];
    char trust_state[32];
    char link_state[32];
} vita_node_peer_t;

typedef struct {
    char task_id[64];
    char origin_node_id[64];
    char target_node_id[64];
    char task_type[64];
    char status[32];
    uint64_t created_unix;
    uint64_t updated_unix;
    char payload_json[256];
} vita_node_task_t;

bool node_core_start(const vita_handoff_t *handoff);
int node_core_peer_count(void);
void node_core_show_peers(void);
void node_core_show_link_proposal(void);
int node_core_pending_proposal_count(void);
bool node_core_handle_link_response(const char *proposal_id, bool approve);

void node_core_poll(void);
void node_core_show_tasks(bool pending_only);
bool node_core_request_peer_status(void);
bool node_core_request_audit_replication(void);

#endif
