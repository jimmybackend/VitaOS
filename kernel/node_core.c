/*
 * kernel/node_core.c
 * VitaNet v0 minimal hosted slice for the current F1B milestone.
 */

#include <vita/audit.h>
#include <vita/console.h>
#include <vita/node.h>

#ifdef VITA_HOSTED
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
#endif

static vita_node_peer_t g_peer;
static bool g_peer_valid = false;
static bool g_link_proposal_pending = false;
static char g_link_proposal_id[16] = "N-1";

#ifdef VITA_HOSTED
static char g_seed_endpoint[64] = "127.0.0.1:47001";

static void mem_zero(void *ptr, unsigned long n) {
    unsigned char *p = (unsigned char *)ptr;
    for (unsigned long i = 0; i < n; ++i) {
        p[i] = 0;
    }
}
#endif

static void copy_text(char *dst, unsigned long cap, const char *src) {
    unsigned long i = 0;

    if (!dst || cap == 0) {
        return;
    }

    if (!src) {
        dst[0] = '\0';
        return;
    }

    while (src[i] && (i + 1) < cap) {
        dst[i] = src[i];
        i++;
    }

    dst[i] = '\0';
}

static bool str_eq(const char *a, const char *b) {
    unsigned long i = 0;

    if (!a || !b) {
        return false;
    }

    while (a[i] && b[i]) {
        if (a[i] != b[i]) {
            return false;
        }
        i++;
    }

    return a[i] == b[i];
}

static void u64_to_dec(uint64_t v, char out[32]) {
    char tmp[32];
    int i = 0;
    int j = 0;

    if (v == 0) {
        out[0] = '0';
        out[1] = '\0';
        return;
    }

    while (v > 0 && i < (int)(sizeof(tmp) - 1)) {
        tmp[i++] = (char)('0' + (v % 10ULL));
        v /= 10ULL;
    }

    while (i > 0) {
        out[j++] = tmp[--i];
    }

    out[j] = '\0';
}

static uint64_t unix_now(void) {
#ifdef VITA_HOSTED
    return (uint64_t)time(NULL);
#else
    return 0;
#endif
}

#ifdef VITA_HOSTED
static bool starts_with(const char *s, const char *prefix) {
    unsigned long i = 0;

    if (!s || !prefix) {
        return false;
    }

    while (prefix[i]) {
        if (s[i] != prefix[i]) {
            return false;
        }
        i++;
    }

    return true;
}

static bool parse_endpoint(const char *text, struct sockaddr_in *out_addr) {
    char ip[64] = {0};
    const char *colon;
    int port;
    unsigned long n = 0;

    if (!text || !out_addr) {
        return false;
    }

    colon = text;
    while (*colon) {
        if (*colon == ':') {
            break;
        }
        colon++;
    }

    if (*colon != ':') {
        return false;
    }

    while (text[n] && (&text[n] != colon) && (n + 1) < sizeof(ip)) {
        ip[n] = text[n];
        n++;
    }
    ip[n] = '\0';

    port = atoi(colon + 1);
    if (port <= 0 || port > 65535) {
        return false;
    }

    mem_zero(out_addr, sizeof(*out_addr));
    out_addr->sin_family = AF_INET;
    out_addr->sin_port = htons((uint16_t)port);

    if (inet_aton(ip, &out_addr->sin_addr) == 0) {
        return false;
    }

    return true;
}

static bool udp_send(int fd, const struct sockaddr_in *to, const char *msg) {
    unsigned long len = 0;

    if (!msg || !to) {
        return false;
    }

    while (msg[len]) {
        len++;
    }

    return sendto(fd, msg, len, 0, (const struct sockaddr *)to, sizeof(*to)) >= 0;
}

static bool udp_recv(int fd, char *out, unsigned long out_cap) {
    fd_set rfds;
    struct timeval tv;
    ssize_t r;

    if (!out || out_cap < 2) {
        return false;
    }

    FD_ZERO(&rfds);
    FD_SET(fd, &rfds);

    tv.tv_sec = 0;
    tv.tv_usec = 350000;

    if (select(fd + 1, &rfds, NULL, NULL, &tv) <= 0) {
        return false;
    }

    r = recvfrom(fd, out, out_cap - 1, 0, NULL, NULL);
    if (r <= 0) {
        return false;
    }

    out[r] = '\0';
    return true;
}

static void send_control_message(const char *endpoint, const char *message) {
    int fd;
    struct sockaddr_in target;

    if (!endpoint || !message) {
        return;
    }

    if (!parse_endpoint(endpoint, &target)) {
        return;
    }

    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        return;
    }

    udp_send(fd, &target, message);
    close(fd);
}

static void ensure_peer_defaults(void) {
    if (!g_peer_valid) {
        mem_zero(&g_peer, sizeof(g_peer));
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
    vita_ai_proposal_t row;

    if (!g_peer_valid || g_link_proposal_pending) {
        return;
    }

    row.proposal_id = g_link_proposal_id;
    row.proposal_type = "link_with_peer";
    row.summary = "Link with discovered peer / Vincular con peer detectado";
    row.rationale = "VitaNet peer discovered with basic capabilities";
    row.risk_level = "medium";
    row.benefit_level = "high";
    row.requires_human_confirmation = true;
    row.status = "PENDING";

    if (audit_persist_ai_proposal(&row)) {
        g_link_proposal_pending = true;
        audit_emit_boot_event("LINK_PROPOSAL_CREATED", g_peer.peer_id);
    }
}

static void handle_message(const char *msg) {
    ensure_peer_defaults();

    if (!msg) {
        return;
    }

    if (starts_with(msg, "HELLO ")) {
        copy_text(g_peer.peer_id, sizeof(g_peer.peer_id), msg + 6);
        g_peer.first_seen_unix = g_peer.first_seen_unix ? g_peer.first_seen_unix : unix_now();
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

    if (starts_with(msg, "LINK_ACCEPT")) {
        copy_text(g_peer.link_state, sizeof(g_peer.link_state), "linked");
        g_peer.last_seen_unix = unix_now();
        audit_upsert_node_peer(&g_peer);
        audit_emit_boot_event("LINK_ACCEPTED_REMOTE", g_peer.peer_id);
        g_link_proposal_pending = false;
        return;
    }

    if (starts_with(msg, "LINK_REJECT")) {
        copy_text(g_peer.link_state, sizeof(g_peer.link_state), "rejected");
        g_peer.last_seen_unix = unix_now();
        audit_upsert_node_peer(&g_peer);
        audit_emit_boot_event("LINK_REJECTED_REMOTE", g_peer.peer_id);
        g_link_proposal_pending = false;
        return;
    }

    if (starts_with(msg, "AUDIT_BLOCK ")) {
        g_peer.last_seen_unix = unix_now();
        audit_upsert_node_peer(&g_peer);
        audit_emit_boot_event("AUDIT_REPLICATION_RECEIVED", g_peer.peer_id);
        return;
    }
}
#endif

bool node_core_start(const vita_handoff_t *handoff) {
    audit_emit_boot_event("VITANET_STARTED", "vitanet started");
    g_peer_valid = false;
    g_link_proposal_pending = false;

#ifdef VITA_HOSTED
    int fd;
    struct sockaddr_in target;
    struct sockaddr_in local;
    char msg[512];
    char block[512];

    if (handoff && handoff->vitanet_seed_endpoint && handoff->vitanet_seed_endpoint[0]) {
        copy_text(g_seed_endpoint, sizeof(g_seed_endpoint), handoff->vitanet_seed_endpoint);
    }

    if (!parse_endpoint(g_seed_endpoint, &target)) {
        audit_emit_boot_event("VITANET_ERROR", "invalid seed endpoint");
        return false;
    }

    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        audit_emit_boot_event("VITANET_ERROR", "udp socket failed");
        return false;
    }

    mem_zero(&local, sizeof(local));
    local.sin_family = AF_INET;
    local.sin_addr.s_addr = htonl(INADDR_ANY);
    local.sin_port = htons(0);

    bind(fd, (struct sockaddr *)&local, sizeof(local));

    udp_send(fd, &target, "HELLO node-local");
    udp_send(fd, &target, "CAPS {\"audit_replication_v0\":true,\"node_task_v0\":true}");
    audit_emit_boot_event("VITANET_HELLO_SENT", "hello and caps sent");

    for (int i = 0; i < 4; ++i) {
        if (!udp_recv(fd, msg, sizeof(msg))) {
            continue;
        }
        audit_emit_boot_event("VITANET_RX", msg);
        handle_message(msg);
    }

    if (g_peer_valid && audit_export_recent_event_block(block, sizeof(block))) {
        char wire[768];

        copy_text(wire, sizeof(wire), "AUDIT_BLOCK ");
        copy_text(wire + 12, sizeof(wire) - 12, block);

        udp_send(fd, &target, wire);
        audit_emit_boot_event("AUDIT_REPLICATION_ATTEMPTED", g_peer.peer_id);
    }

    close(fd);
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
    u64_to_dec(g_peer.first_seen_unix, num);
    console_write_line(num);

    console_write_line("last_seen_unix:");
    u64_to_dec(g_peer.last_seen_unix, num);
    console_write_line(num);

    if (g_link_proposal_pending) {
        console_write_line("pending_link_proposal:");
        console_write_line(g_link_proposal_id);
    }
}

void node_core_show_link_proposal(void) {
    if (!g_link_proposal_pending || !g_peer_valid) {
        return;
    }

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
    console_write_line("Create initial cooperative link for audit replication trials.");

    console_write_line("7. Estado de auditoria / Audit state:");
    console_write_line("PENDING");

    audit_emit_boot_event("AI_PROPOSAL_SHOWN", g_link_proposal_id);
}

int node_core_pending_proposal_count(void) {
    return g_link_proposal_pending ? 1 : 0;
}

bool node_core_handle_link_response(const char *proposal_id, bool approve) {
    if (!proposal_id || !g_link_proposal_pending || !g_peer_valid) {
        return false;
    }

    if (!str_eq(proposal_id, g_link_proposal_id)) {
        return false;
    }

    if (approve) {
        copy_text(g_peer.link_state, sizeof(g_peer.link_state), "linked");
        audit_emit_boot_event("LINK_ACCEPTED", g_peer.peer_id);
#ifdef VITA_HOSTED
        send_control_message(g_seed_endpoint, "LINK_ACCEPT node-local");
#endif
    } else {
        copy_text(g_peer.link_state, sizeof(g_peer.link_state), "rejected");
        audit_emit_boot_event("LINK_REJECTED", g_peer.peer_id);
#ifdef VITA_HOSTED
        send_control_message(g_seed_endpoint, "LINK_REJECT node-local");
#endif
    }

    g_peer.last_seen_unix = unix_now();
    audit_upsert_node_peer(&g_peer);
    g_link_proposal_pending = false;
    return true;
}
