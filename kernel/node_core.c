/*
 * kernel/node_core.c
 * Cooperative node core for VitaNet.
 *
 * Responsibilities:
 * - initialize local node identity
 * - discover peers
 * - keep heartbeat
 * - expose peer presence to AI core
 * - coordinate simple tasks
 */

#include <stdint.h>
#include <stdbool.h>

static bool g_has_peers = false;

void audit_emit_boot_event(const char *event_type, const char *summary);

void node_core_init(const void *boot_ctx) {
    (void)boot_ctx;
    /* TODO:
     * - init VitaNet discovery over Ethernet
     * - optionally init Wi-Fi discovery if driver ready
     * - persist discovered peers into node_peer
     */
    g_has_peers = false;
    audit_emit_boot_event("NODE_CORE_ONLINE", "VitaNet core initialized");
}

bool node_core_has_peers(void) {
    return g_has_peers;
}

/* TODO:
 * - node_discover()
 * - node_publish_caps()
 * - node_handle_hello()
 * - node_replicate_audit()
 * - node_assign_task()
 */
