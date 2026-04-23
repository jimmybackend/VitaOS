/*
 * kernel/node_core.c
 * VitaNet v0/v0.1 minimal hosted slice for F1B.
 */

#include <vita/audit.h>
#include <vita/console.h>
#include <vita/node.h>
#include <vita/proposal.h>

#ifdef VITA_HOSTED
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
#endif

#define NODE_MAX_TASKS 24

static vita_node_peer_t g_peer;
static bool g_peer_valid = false;
static bool g_link_proposal_pending = false;
static char g_link_proposal_id[16] = "N-1";

static vita_node_task_t g_tasks[NODE_MAX_TASKS];
static int g_task_count = 0;

#ifdef VITA_HOSTED
static char g_seed_endpoint[64] = "127.0.0.1:47001";
static int g_udp_fd = -1;
static struct sockaddr_in g_target_addr;
#endif

static void copy_text(char *dst, unsigned long cap, const char *src) {
    unsigned long i = 0;
    if (!dst || cap == 0) return;
    if (!src) {
        dst[0] = '\0';
        return;
    }
    while (src[i] && i + 1 < cap) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static bool str_eq(const char *a, const char *b) {
    unsigned long i = 0;
    if (!a || !b) return false;
    while (a[i] && b[i]) {
        if (a[i] != b[i]) return false;
        i++;
    }
    return a[i] == b[i];
}


#ifdef VITA_HOSTED
static bool starts_with(const char *s, const char *prefix) {
    unsigned long i = 0;
    if (!s || !prefix) return false;
    while (prefix[i]) {
        if (s[i] != prefix[i]) return false;
        i++;
    }
    return true;
}
#endif

static void u64_to_dec(uint64_t v, char out[32]) {
    char tmp[32];
    int i = 0;
    int j = 0;
    if (v == 0) {
        out[0] = '0'; out[1] = '\0'; return;
    }
    while (v > 0 && i < (int)sizeof(tmp) - 1) {
        tmp[i++] = (char)('0' + (v % 10));
        v /= 10;
    }
    while (i > 0) out[j++] = tmp[--i];
    out[j] = '\0';
}

static uint64_t unix_now(void) {
#ifdef VITA_HOSTED
    return (uint64_t)time(NULL);
#else
    return 0;
#endif
}

static void mk_task_id(int n, char out[64]) {
    out[0] = 'T'; out[1] = '-';
    if (n >= 100) {
        out[2] = (char)('0' + ((n / 100) % 10));
        out[3] = (char)('0' + ((n / 10) % 10));
        out[4] = (char)('0' + (n % 10));
        out[5] = '\0';
    } else if (n >= 10) {
        out[2] = (char)('0' + (n / 10));
        out[3] = (char)('0' + (n % 10));
        out[4] = '\0';
    } else {
        out[2] = (char)('0' + n);
        out[3] = '\0';
    }
}


#ifdef VITA_HOSTED
static vita_node_task_t *task_find(const char *task_id) {
    for (int i = 0; i < g_task_count; ++i) {
        if (str_eq(g_tasks[i].task_id, task_id)) return &g_tasks[i];
    }
    return 0;
}
#endif

static vita_node_task_t *task_create(const char *type, const char *payload, const char *target) {
    vita_node_task_t *t;
    if (g_task_count >= NODE_MAX_TASKS) return 0;
    t = &g_tasks[g_task_count++];
    mk_task_id(g_task_count, t->task_id);
    copy_text(t->origin_node_id, sizeof(t->origin_node_id), "node-local");
    copy_text(t->target_node_id, sizeof(t->target_node_id), target ? target : "peer-unknown");
    copy_text(t->task_type, sizeof(t->task_type), type);
    copy_text(t->status, sizeof(t->status), "created");
    t->created_unix = unix_now();
    t->updated_unix = t->created_unix;
    copy_text(t->payload_json, sizeof(t->payload_json), payload ? payload : "{}");
    audit_upsert_node_task(t);
    audit_emit_boot_event("NODE_TASK_CREATED", t->task_id);
    return t;
}

static void task_set_status(vita_node_task_t *t, const char *status) {
    if (!t || !status) return;
    copy_text(t->status, sizeof(t->status), status);
    t->updated_unix = unix_now();
    audit_upsert_node_task(t);
}

#ifdef VITA_HOSTED
static bool parse_endpoint(const char *text, struct sockaddr_in *out_addr) {
    char ip[64] = {0};
    const char *colon = text;
    int port;
    unsigned long n = 0;

    if (!text || !out_addr) return false;
    while (*colon) {
        if (*colon == ':') break;
        colon++;
    }
    if (*colon != ':') return false;
    while (text[n] && &text[n] != colon && n + 1 < sizeof(ip)) {
        ip[n] = text[n];
        n++;
    }
    ip[n] = '\0';
    port = atoi(colon + 1);
    if (port <= 0 || port > 65535) return false;

    out_addr->sin_family = AF_INET;
    out_addr->sin_port = htons((uint16_t)port);
    if (inet_aton(ip, &out_addr->sin_addr) == 0) return false;
    return true;
}

static bool udp_send_text(const char *msg) {
    unsigned long len = 0;
    if (g_udp_fd < 0 || !msg) return false;
    while (msg[len]) len++;
    return sendto(g_udp_fd, msg, len, 0, (const struct sockaddr *)&g_target_addr, sizeof(g_target_addr)) >= 0;
}

static bool udp_recv_text(char *out, unsigned long out_cap, int timeout_ms) {
    fd_set rfds;
    struct timeval tv;
    ssize_t r;
    if (g_udp_fd < 0 || !out || out_cap < 2) return false;

    FD_ZERO(&rfds);
    FD_SET(g_udp_fd, &rfds);
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    if (select(g_udp_fd + 1, &rfds, NULL, NULL, &tv) <= 0) return false;

    r = recvfrom(g_udp_fd, out, out_cap - 1, 0, NULL, NULL);
    if (r <= 0) return false;
    out[r] = '\0';
    return true;
}
#endif


#ifdef VITA_HOSTED
static void ensure_peer_defaults(void) {
    if (!g_peer_valid) {
        copy_text(g_peer.peer_id, sizeof(g_peer.peer_id), "peer-unknown");
        copy_text(g_peer.transport, sizeof(g_peer.transport), "udp/ipv4-hosted");
        copy_text(g_peer.capabilities_json, sizeof(g_peer.capabilities_json), "{}");
        copy_text(g_peer.trust_state, sizeof(g_peer.trust_state), "observed");
        copy_text(g_peer.link_state, sizeof(g_peer.link_state), "discovered");
        g_peer.first_seen_unix = unix_now();
        g_peer.last_seen_unix = g_peer.first_seen_unix;
        g_peer_valid = true;
    }
}

static void create_link_proposal_if_needed(void) {
    if (!g_peer_valid || g_link_proposal_pending) return;
    {
        vita_ai_proposal_t row = {
            .proposal_id = g_link_proposal_id,
            .proposal_type = "link_with_peer",
            .summary = "Link with discovered peer / Vincular con peer detectado",
            .rationale = "VitaNet peer discovered with basic capabilities",
            .risk_level = "medium",
            .benefit_level = "high",
            .requires_human_confirmation = true,
            .status = "PENDING",
        };
        if (audit_persist_ai_proposal(&row)) {
            g_link_proposal_pending = true;
            audit_emit_boot_event("LINK_PROPOSAL_CREATED", g_peer.peer_id);
        }
    }
}
#endif

#ifdef VITA_HOSTED
static void parse_audit_block(const char *origin, const char *payload) {
    char item[256];
    unsigned long i = 0;
    unsigned long j = 0;
    if (!payload) return;

    audit_emit_boot_event("AUDIT_REPLICATION_RECEIVED", origin ? origin : "peer");

    while (1) {
        if (payload[i] == ';' || payload[i] == '\0') {
            item[j] = '\0';
            if (j > 0) {
                char *p1, *p2, *p3;
                uint64_t seq = 0;
                p1 = item;
                while (*p1 && *p1 != ':') p1++;
                if (*p1 == ':') {
                    *p1++ = '\0';
                    p2 = p1;
                    while (*p2 && *p2 != ':') p2++;
                    if (*p2 == ':') {
                        *p2++ = '\0';
                        p3 = p2;
                        {
                            char *sp = item;
                            while (*sp) { seq = seq * 10 + (uint64_t)(*sp - '0'); sp++; }
                        }
                        if (audit_import_remote_event(origin ? origin : "peer", "remote-boot", seq, p1, p3)) {
                            audit_emit_boot_event("AUDIT_REPLICATION_PERSISTED", p3);
                        } else {
                            audit_emit_boot_event("AUDIT_REPLICATION_FAILED", p3);
                        }
                    }
                }
            }
            if (payload[i] == '\0') break;
            j = 0;
            i++;
            continue;
        }
        if (j + 1 < sizeof(item)) item[j++] = payload[i];
        i++;
    }
}

static void handle_incoming_message(const char *msg) {
    if (!msg) return;
    ensure_peer_defaults();

    if (starts_with(msg, "HELLO ")) {
        copy_text(g_peer.peer_id, sizeof(g_peer.peer_id), msg + 6);
        g_peer.last_seen_unix = unix_now();
        audit_upsert_node_peer(&g_peer);
        audit_emit_boot_event("PEER_DISCOVERED", g_peer.peer_id);
        create_link_proposal_if_needed();
        return;
    }

    if (starts_with(msg, "CAPS ")) {
        copy_text(g_peer.capabilities_json, sizeof(g_peer.capabilities_json), msg + 5);
        g_peer.last_seen_unix = unix_now();
        audit_upsert_node_peer(&g_peer);
        audit_emit_boot_event("PEER_CAPABILITIES_RECEIVED", g_peer.peer_id);
        return;
    }

    if (starts_with(msg, "HEARTBEAT")) {
        g_peer.last_seen_unix = unix_now();
        audit_upsert_node_peer(&g_peer);
        audit_emit_boot_event("HEARTBEAT_RECEIVED", g_peer.peer_id);
        return;
    }


    if (starts_with(msg, "TASK ")) {
        char work[512];
        char *type;
        char *id;
        char *payload;
        vita_node_task_t *t;
        copy_text(work, sizeof(work), msg + 5);
        type = work;
        id = type;
        while (*id && *id != '|') id++;
        if (*id != '|') return;
        *id++ = '\0';
        payload = id;
        while (*payload && *payload != '|') payload++;
        if (*payload != '|') return;
        *payload++ = '\0';

        t = task_find(id);
        if (!t && g_task_count < NODE_MAX_TASKS) {
            t = &g_tasks[g_task_count++];
            copy_text(t->task_id, sizeof(t->task_id), id);
            copy_text(t->origin_node_id, sizeof(t->origin_node_id), g_peer.peer_id);
            copy_text(t->target_node_id, sizeof(t->target_node_id), "node-local");
            copy_text(t->task_type, sizeof(t->task_type), type);
            copy_text(t->payload_json, sizeof(t->payload_json), payload);
            t->created_unix = unix_now();
        }
        if (!t) return;
        task_set_status(t, "accepted");
        audit_emit_boot_event("NODE_TASK_ACCEPTED", t->task_id);
        audit_upsert_node_task(t);
#ifdef VITA_HOSTED
        {
            char ack[128];
            snprintf(ack, sizeof(ack), "TASK_ACK %s", t->task_id);
            udp_send_text(ack);
        }
#endif
        if (str_eq(t->task_type, "peer_status_request")) {
            copy_text(t->status, sizeof(t->status), "done");
            t->updated_unix = unix_now();
            audit_upsert_node_task(t);
            audit_emit_boot_event("PEER_STATUS_REQUESTED", t->task_id);
            audit_emit_boot_event("NODE_TASK_COMPLETED", t->task_id);
#ifdef VITA_HOSTED
            {
                char done[256];
                snprintf(done, sizeof(done), "TASK_DONE %s", t->task_id);
                udp_send_text(done);
            }
#endif
            return;
        }
        if (str_eq(t->task_type, "audit_replicate_range")) {
            char block[512];
            copy_text(t->status, sizeof(t->status), "done");
            t->updated_unix = unix_now();
            audit_upsert_node_task(t);
            audit_emit_boot_event("AUDIT_REPLICATION_STARTED", t->task_id);
    audit_emit_boot_event("AUDIT_REPLICATION_ATTEMPTED", t->task_id);
            if (audit_export_recent_event_block(block, sizeof(block))) {
#ifdef VITA_HOSTED
                char wire[768];
                snprintf(wire, sizeof(wire), "AUDIT_BLOCK %s", block);
                udp_send_text(wire);
#endif
                audit_emit_boot_event("AUDIT_REPLICATION_PERSISTED", t->task_id);
            } else {
                audit_emit_boot_event("AUDIT_REPLICATION_FAILED", t->task_id);
            }
            audit_emit_boot_event("NODE_TASK_COMPLETED", t->task_id);
#ifdef VITA_HOSTED
            {
                char done[256];
                snprintf(done, sizeof(done), "TASK_DONE %s", t->task_id);
                udp_send_text(done);
            }
#endif
            return;
        }
        task_set_status(t, "failed");
        audit_emit_boot_event("NODE_TASK_REJECTED", t->task_id);
#ifdef VITA_HOSTED
        {
            char rej[128];
            snprintf(rej, sizeof(rej), "TASK_REJECT %s", t->task_id);
            udp_send_text(rej);
        }
#endif
        return;
    }

    if (starts_with(msg, "TASK_ACK ")) {
        const char *id = msg + 9;
        vita_node_task_t *t = task_find(id);
        if (t) {
            task_set_status(t, "accepted");
            audit_emit_boot_event("NODE_TASK_ACCEPTED", id);
        }
        return;
    }

    if (starts_with(msg, "TASK_REJECT ")) {
        const char *id = msg + 12;
        vita_node_task_t *t = task_find(id);
        if (t) {
            task_set_status(t, "rejected");
            audit_emit_boot_event("NODE_TASK_REJECTED", id);
        }
        return;
    }

    if (starts_with(msg, "TASK_DONE ")) {
        const char *id = msg + 10;
        vita_node_task_t *t = task_find(id);
        if (t) {
            task_set_status(t, "done");
            audit_emit_boot_event("NODE_TASK_COMPLETED", id);
            if (str_eq(t->task_type, "peer_status_request")) {
                audit_emit_boot_event("PEER_STATUS_RECEIVED", id);
            }
        }
        return;
    }

    if (starts_with(msg, "AUDIT_BLOCK ")) {
        parse_audit_block(g_peer.peer_id, msg + 12);
        return;
    }
}

#endif

void node_core_poll(void) {
#ifdef VITA_HOSTED
    char msg[512];
    int loops = 0;
    while (loops < 8 && udp_recv_text(msg, sizeof(msg), 80)) {
        handle_incoming_message(msg);
        loops++;
    }
#endif
}

bool node_core_start(const vita_handoff_t *handoff) {
    audit_emit_boot_event("VITANET_STARTED", "vitanet started");
    g_peer_valid = false;
    g_link_proposal_pending = false;
    g_task_count = 0;

#ifdef VITA_HOSTED
    const char *endpoint = (handoff && handoff->vitanet_seed_endpoint) ? handoff->vitanet_seed_endpoint : "127.0.0.1:47001";
    struct sockaddr_in local;

    copy_text(g_seed_endpoint, sizeof(g_seed_endpoint), endpoint);
    if (!parse_endpoint(endpoint, &g_target_addr)) {
        audit_emit_boot_event("VITANET_ERROR", "invalid seed endpoint");
        return false;
    }

    g_udp_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (g_udp_fd < 0) {
        audit_emit_boot_event("VITANET_ERROR", "udp socket failed");
        return false;
    }

    local.sin_family = AF_INET;
    local.sin_addr.s_addr = htonl(INADDR_ANY);
    local.sin_port = htons(0);
    bind(g_udp_fd, (struct sockaddr *)&local, sizeof(local));

    udp_send_text("HELLO node-local");
    udp_send_text("CAPS {\"audit_replication_v0\":true,\"node_task_v0\":true}");
    node_core_poll();

    return g_peer_valid;
#else
    (void)handoff;
    audit_emit_boot_event("VITANET_LIMITED", "vitanet hosted adapter only");
    return false;
#endif
}

int node_core_peer_count(void) {
    return g_peer_valid ? 1 : 0;
}

void node_core_show_peers(void) {
    char num[32];
    node_core_poll();
    if (!g_peer_valid) {
        console_write_line("No peers discovered / Sin peers detectados");
        return;
    }
    console_write_line("Peer summary / Resumen de peer:");
    console_write_line(g_peer.peer_id);
    console_write_line("transport:");
    console_write_line(g_peer.transport);
    console_write_line("capabilities_json:");
    console_write_line(g_peer.capabilities_json);
    console_write_line("trust_state:");
    console_write_line(g_peer.trust_state);
    console_write_line("link_state:");
    console_write_line(g_peer.link_state);
    console_write_line("first_seen_unix:");
    u64_to_dec(g_peer.first_seen_unix, num); console_write_line(num);
    console_write_line("last_seen_unix:");
    u64_to_dec(g_peer.last_seen_unix, num); console_write_line(num);
}

void node_core_show_link_proposal(void) {
    if (!g_link_proposal_pending || !g_peer_valid) return;
    console_write_line("--- Proposal / Propuesta ---");
    console_write_line(g_link_proposal_id);
    console_write_line("1. Resumen / Summary:");
    console_write_line("Link with discovered peer / Vincular con peer detectado");
    console_write_line("2. Riesgos inmediatos / Immediate risks:");
    console_write_line("medium");
    console_write_line("3. Preguntas criticas / Critical questions:");
    console_write_line("Is this peer trusted for initial cooperation?");
    console_write_line("4. Acciones inmediatas sugeridas / Suggested actions:");
    console_write_line("approve N-1 | reject N-1");
    console_write_line("5. Recursos del sistema / System resources:");
    console_write_line(g_peer.capabilities_json);
    console_write_line("6. Propuesta del sistema / System proposal:");
    console_write_line("Create initial cooperative link for task + audit replication.");
    console_write_line("7. Estado de auditoria / Audit state:");
    console_write_line("PENDING");
    audit_emit_boot_event("AI_PROPOSAL_SHOWN", g_link_proposal_id);
}

int node_core_pending_proposal_count(void) {
    return g_link_proposal_pending ? 1 : 0;
}

bool node_core_handle_link_response(const char *proposal_id, bool approve) {
    if (!proposal_id || !g_link_proposal_pending || !g_peer_valid) return false;
    if (!str_eq(proposal_id, g_link_proposal_id)) return false;

    if (approve) {
        copy_text(g_peer.link_state, sizeof(g_peer.link_state), "linked");
        audit_emit_boot_event("LINK_ACCEPTED", g_peer.peer_id);
#ifdef VITA_HOSTED
        udp_send_text("LINK_ACCEPT node-local");
#endif
    } else {
        copy_text(g_peer.link_state, sizeof(g_peer.link_state), "rejected");
        audit_emit_boot_event("LINK_REJECTED", g_peer.peer_id);
#ifdef VITA_HOSTED
        udp_send_text("LINK_REJECT node-local");
#endif
    }
    g_peer.last_seen_unix = unix_now();
    audit_upsert_node_peer(&g_peer);
    g_link_proposal_pending = false;
    if (approve) {
        node_core_request_peer_status();
    }
    return true;
}

void node_core_show_tasks(bool pending_only) {
    char num[32];
    node_core_poll();
    for (int i = 0; i < g_task_count; ++i) {
        vita_node_task_t *t = &g_tasks[i];
        if (pending_only && !(str_eq(t->status, "created") || str_eq(t->status, "sent") || str_eq(t->status, "accepted"))) {
            continue;
        }
        console_write_line("task_id:"); console_write_line(t->task_id);
        console_write_line("task_type:"); console_write_line(t->task_type);
        console_write_line("status:"); console_write_line(t->status);
        console_write_line("target_node_id:"); console_write_line(t->target_node_id);
        console_write_line("updated_unix:"); u64_to_dec(t->updated_unix, num); console_write_line(num);
    }
}

static bool task_send_wire(vita_node_task_t *t) {
    if (!t || !g_peer_valid || !str_eq(g_peer.link_state, "linked")) return false;
#ifdef VITA_HOSTED
    {
        char msg[512];
        snprintf(msg, sizeof(msg), "TASK %s|%s|%s", t->task_type, t->task_id, t->payload_json);
        if (!udp_send_text(msg)) return false;
    }
    task_set_status(t, "sent");
    audit_emit_boot_event("NODE_TASK_SENT", t->task_id);
    return true;
#else
    return false;
#endif
}

bool node_core_request_peer_status(void) {
    vita_node_task_t *t;
    if (!g_peer_valid) return false;
    t = task_create("peer_status_request", "{\"request\":\"peer_status\"}", g_peer.peer_id);
    if (!t) return false;
    audit_emit_boot_event("PEER_STATUS_REQUESTED", t->task_id);
    if (!task_send_wire(t)) {
        task_set_status(t, "failed");
        return false;
    }
    node_core_poll();
    return true;
}

bool node_core_request_audit_replication(void) {
    vita_node_task_t *t;
    char block[512];
    if (!g_peer_valid) return false;
    t = task_create("audit_replicate_range", "{\"range\":\"recent\",\"limit\":3}", g_peer.peer_id);
    if (!t) return false;
    audit_emit_boot_event("AUDIT_REPLICATION_STARTED", t->task_id);
    audit_emit_boot_event("AUDIT_REPLICATION_ATTEMPTED", t->task_id);
    if (!task_send_wire(t)) {
        task_set_status(t, "failed");
        audit_emit_boot_event("AUDIT_REPLICATION_FAILED", t->task_id);
        return false;
    }
    if (audit_export_recent_event_block(block, sizeof(block))) {
#ifdef VITA_HOSTED
        char msg[768];
        snprintf(msg, sizeof(msg), "AUDIT_BLOCK %s", block);
        udp_send_text(msg);
#endif
    }
    node_core_poll();
    return true;
}
