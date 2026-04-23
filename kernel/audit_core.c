/*
 * kernel/audit_core.c
 * Audit subsystem for the current F1A/F1B slice.
 *
 * Hosted build:
 * - persistent SQLite backend
 * - append-mostly audit_event hash chain
 * - boot_session + hardware_snapshot
 * - ai_proposal + human_response
 * - node_peer upsert
 * - recent event block export for basic replication trials
 *
 * UEFI/freestanding build:
 * - explicit stubs until persistent backend exists there
 */

#include <vita/audit.h>

#ifdef VITA_HOSTED

#include <sqlite3.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define AUDIT_HASH_HEX_LEN 64
#define AUDIT_EARLY_MAX_EVENTS 32
#define AUDIT_RECENT_BLOCK_EVENTS 8

typedef struct {
    char event_type[64];
    char summary[256];
    uint64_t monotonic_ns;
} early_event_t;

typedef struct {
    bool persistent_ready;
    uint64_t event_seq;
    uint64_t boot_started_unix;
    char boot_id[64];
    char node_id[64];
    char arch_name[32];
    char prev_hash[AUDIT_HASH_HEX_LEN + 1];
    early_event_t early_events[AUDIT_EARLY_MAX_EVENTS];
    uint32_t early_count;
    sqlite3 *db;
} audit_state_t;

static audit_state_t g_audit = {0};

typedef struct {
    uint8_t data[64];
    uint32_t datalen;
    uint64_t bitlen;
    uint32_t state[8];
} sha256_ctx_t;

#define ROTR(x,n) (((x) >> (n)) | ((x) << (32 - (n))))
#define CH(x,y,z) (((x) & (y)) ^ (~(x) & (z)))
#define MAJ(x,y,z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define EP0(x) (ROTR(x,2) ^ ROTR(x,13) ^ ROTR(x,22))
#define EP1(x) (ROTR(x,6) ^ ROTR(x,11) ^ ROTR(x,25))
#define SIG0(x) (ROTR(x,7) ^ ROTR(x,18) ^ ((x) >> 3))
#define SIG1(x) (ROTR(x,17) ^ ROTR(x,19) ^ ((x) >> 10))

static const uint32_t ktab[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};

static void sha256_transform(sha256_ctx_t *ctx, const uint8_t data[]);
static void sha256_init(sha256_ctx_t *ctx);
static void sha256_update(sha256_ctx_t *ctx, const uint8_t data[], size_t len);
static void sha256_final(sha256_ctx_t *ctx, uint8_t hash[]);

static uint64_t monotonic_now_ns(void) {
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

static uint64_t unix_now(void) {
    return (uint64_t)time(NULL);
}

static void mem_zero(void *ptr, unsigned long n) {
    unsigned char *p = (unsigned char *)ptr;
    for (unsigned long i = 0; i < n; ++i) {
        p[i] = 0;
    }
}

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

static const char *safe_text(const char *text, const char *fallback) {
    if (text && text[0]) {
        return text;
    }
    return fallback;
}

static void sha256_transform(sha256_ctx_t *ctx, const uint8_t data[]) {
    uint32_t a, b, c, d, e, f, g, h, m[64], t1, t2;
    uint32_t i;
    uint32_t j;

    for (i = 0, j = 0; i < 16; ++i, j += 4) {
        m[i] = ((uint32_t)data[j] << 24) |
               ((uint32_t)data[j + 1] << 16) |
               ((uint32_t)data[j + 2] << 8) |
               ((uint32_t)data[j + 3]);
    }

    for (i = 16; i < 64; ++i) {
        m[i] = SIG1(m[i - 2]) + m[i - 7] + SIG0(m[i - 15]) + m[i - 16];
    }

    a = ctx->state[0];
    b = ctx->state[1];
    c = ctx->state[2];
    d = ctx->state[3];
    e = ctx->state[4];
    f = ctx->state[5];
    g = ctx->state[6];
    h = ctx->state[7];

    for (i = 0; i < 64; ++i) {
        t1 = h + EP1(e) + CH(e, f, g) + ktab[i] + m[i];
        t2 = EP0(a) + MAJ(a, b, c);

        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }

    ctx->state[0] += a;
    ctx->state[1] += b;
    ctx->state[2] += c;
    ctx->state[3] += d;
    ctx->state[4] += e;
    ctx->state[5] += f;
    ctx->state[6] += g;
    ctx->state[7] += h;
}

static void sha256_init(sha256_ctx_t *ctx) {
    ctx->datalen = 0;
    ctx->bitlen = 0;
    ctx->state[0] = 0x6a09e667;
    ctx->state[1] = 0xbb67ae85;
    ctx->state[2] = 0x3c6ef372;
    ctx->state[3] = 0xa54ff53a;
    ctx->state[4] = 0x510e527f;
    ctx->state[5] = 0x9b05688c;
    ctx->state[6] = 0x1f83d9ab;
    ctx->state[7] = 0x5be0cd19;
}

static void sha256_update(sha256_ctx_t *ctx, const uint8_t data[], size_t len) {
    size_t i;

    for (i = 0; i < len; ++i) {
        ctx->data[ctx->datalen++] = data[i];
        if (ctx->datalen == 64) {
            sha256_transform(ctx, ctx->data);
            ctx->bitlen += 512;
            ctx->datalen = 0;
        }
    }
}

static void sha256_final(sha256_ctx_t *ctx, uint8_t hash[]) {
    uint32_t i = ctx->datalen;

    if (ctx->datalen < 56) {
        ctx->data[i++] = 0x80;
        while (i < 56) {
            ctx->data[i++] = 0x00;
        }
    } else {
        ctx->data[i++] = 0x80;
        while (i < 64) {
            ctx->data[i++] = 0x00;
        }
        sha256_transform(ctx, ctx->data);
        memset(ctx->data, 0, 56);
    }

    ctx->bitlen += (uint64_t)ctx->datalen * 8ULL;
    ctx->data[63] = (uint8_t)(ctx->bitlen);
    ctx->data[62] = (uint8_t)(ctx->bitlen >> 8);
    ctx->data[61] = (uint8_t)(ctx->bitlen >> 16);
    ctx->data[60] = (uint8_t)(ctx->bitlen >> 24);
    ctx->data[59] = (uint8_t)(ctx->bitlen >> 32);
    ctx->data[58] = (uint8_t)(ctx->bitlen >> 40);
    ctx->data[57] = (uint8_t)(ctx->bitlen >> 48);
    ctx->data[56] = (uint8_t)(ctx->bitlen >> 56);
    sha256_transform(ctx, ctx->data);

    for (i = 0; i < 4; ++i) {
        hash[i]      = (ctx->state[0] >> (24 - i * 8)) & 0xff;
        hash[i + 4]  = (ctx->state[1] >> (24 - i * 8)) & 0xff;
        hash[i + 8]  = (ctx->state[2] >> (24 - i * 8)) & 0xff;
        hash[i + 12] = (ctx->state[3] >> (24 - i * 8)) & 0xff;
        hash[i + 16] = (ctx->state[4] >> (24 - i * 8)) & 0xff;
        hash[i + 20] = (ctx->state[5] >> (24 - i * 8)) & 0xff;
        hash[i + 24] = (ctx->state[6] >> (24 - i * 8)) & 0xff;
        hash[i + 28] = (ctx->state[7] >> (24 - i * 8)) & 0xff;
    }
}

static void hash_hex(const char *payload, char out_hex[AUDIT_HASH_HEX_LEN + 1]) {
    static const char *hex = "0123456789abcdef";
    uint8_t digest[32];
    sha256_ctx_t ctx;
    int i;

    sha256_init(&ctx);
    sha256_update(&ctx, (const uint8_t *)payload, strlen(payload));
    sha256_final(&ctx, digest);

    for (i = 0; i < 32; ++i) {
        out_hex[i * 2] = hex[(digest[i] >> 4) & 0xF];
        out_hex[i * 2 + 1] = hex[digest[i] & 0xF];
    }
    out_hex[64] = '\0';
}

static bool exec_sql_file(sqlite3 *db, const char *path) {
    FILE *f;
    long sz;
    char *sql;
    char *errmsg = NULL;
    int rc;

    f = fopen(path, "rb");
    if (!f) {
        return false;
    }

    fseek(f, 0, SEEK_END);
    sz = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (sz <= 0) {
        fclose(f);
        return false;
    }

    sql = (char *)malloc((size_t)sz + 1U);
    if (!sql) {
        fclose(f);
        return false;
    }

    if (fread(sql, 1, (size_t)sz, f) != (size_t)sz) {
        free(sql);
        fclose(f);
        return false;
    }

    sql[sz] = '\0';
    fclose(f);

    rc = sqlite3_exec(db, sql, NULL, NULL, &errmsg);
    free(sql);

    if (rc != SQLITE_OK) {
        sqlite3_free(errmsg);
        return false;
    }

    return true;
}

static bool bind_text_or_null(sqlite3_stmt *st, int idx, const char *text) {
    if (text && text[0]) {
        return sqlite3_bind_text(st, idx, text, -1, SQLITE_TRANSIENT) == SQLITE_OK;
    }
    return sqlite3_bind_null(st, idx) == SQLITE_OK;
}

static bool insert_boot_session(void) {
    const char *sql =
        "INSERT INTO boot_session "
        "(boot_id,node_id,arch,boot_unix,boot_monotonic_ns,mode,audit_state,language_mode) "
        "VALUES (?1,?2,?3,?4,?5,?6,?7,'es,en');";
    sqlite3_stmt *st = NULL;
    bool ok;

    if (!g_audit.db) {
        return false;
    }

    if (sqlite3_prepare_v2(g_audit.db, sql, -1, &st, NULL) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(st, 1, g_audit.boot_id, -1, SQLITE_STATIC);
    sqlite3_bind_text(st, 2, g_audit.node_id, -1, SQLITE_STATIC);
    sqlite3_bind_text(st, 3, g_audit.arch_name, -1, SQLITE_STATIC);
    sqlite3_bind_int64(st, 4, (sqlite3_int64)g_audit.boot_started_unix);
    sqlite3_bind_int64(st, 5, (sqlite3_int64)monotonic_now_ns());
    sqlite3_bind_text(st, 6, "BOOTSTRAP", -1, SQLITE_STATIC);
    sqlite3_bind_text(st, 7, "READY", -1, SQLITE_STATIC);

    ok = sqlite3_step(st) == SQLITE_DONE;
    sqlite3_finalize(st);
    return ok;
}

static bool insert_event_row(uint64_t seq,
                             const char *etype,
                             const char *summary,
                             uint64_t mono_ns,
                             const char *prev_hash,
                             const char *event_hash) {
    const char *sql =
        "INSERT INTO audit_event "
        "(boot_id,event_seq,event_type,severity,actor_type,actor_id,target_type,target_id,summary,details_json,monotonic_ns,rtc_unix,prev_hash,event_hash) "
        "VALUES (?1,?2,?3,'INFO','SYSTEM',NULL,NULL,NULL,?4,'{}',?5,?6,?7,?8);";
    sqlite3_stmt *st = NULL;
    bool ok;

    if (!g_audit.db) {
        return false;
    }

    if (sqlite3_prepare_v2(g_audit.db, sql, -1, &st, NULL) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(st, 1, g_audit.boot_id, -1, SQLITE_STATIC);
    sqlite3_bind_int64(st, 2, (sqlite3_int64)seq);
    sqlite3_bind_text(st, 3, etype, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 4, summary, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(st, 5, (sqlite3_int64)mono_ns);
    sqlite3_bind_int64(st, 6, (sqlite3_int64)unix_now());
    sqlite3_bind_text(st, 7, prev_hash, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 8, event_hash, -1, SQLITE_TRANSIENT);

    ok = sqlite3_step(st) == SQLITE_DONE;
    sqlite3_finalize(st);
    return ok;
}

static bool append_event_internal(const char *event_type,
                                  const char *summary,
                                  uint64_t monotonic_ns) {
    char payload[1024];
    char event_hash[AUDIT_HASH_HEX_LEN + 1];
    const char *etype = safe_text(event_type, "UNSPECIFIED");
    const char *sum = safe_text(summary, "");
    const char *prev;

    if (!g_audit.persistent_ready || !g_audit.db) {
        return false;
    }

    g_audit.event_seq++;
    prev = g_audit.prev_hash[0] ? g_audit.prev_hash : "";

    snprintf(payload,
             sizeof(payload),
             "%s|%llu|%s|INFO|SYSTEM|%s|{}|%llu|%s",
             g_audit.boot_id,
             (unsigned long long)g_audit.event_seq,
             etype,
             sum,
             (unsigned long long)monotonic_ns,
             prev);

    hash_hex(payload, event_hash);

    if (!insert_event_row(g_audit.event_seq, etype, sum, monotonic_ns, prev, event_hash)) {
        g_audit.event_seq--;
        return false;
    }

    copy_text(g_audit.prev_hash, sizeof(g_audit.prev_hash), event_hash);
    return true;
}

void audit_early_buffer_init(void) {
    if (g_audit.db) {
        sqlite3_close(g_audit.db);
    }
    mem_zero(&g_audit, sizeof(g_audit));
}

void audit_emit_boot_event(const char *event_type, const char *summary) {
    uint64_t mono = monotonic_now_ns();

    if (!g_audit.persistent_ready) {
        if (g_audit.early_count < AUDIT_EARLY_MAX_EVENTS) {
            early_event_t *ev = &g_audit.early_events[g_audit.early_count++];
            copy_text(ev->event_type, sizeof(ev->event_type), safe_text(event_type, "UNSPECIFIED"));
            copy_text(ev->summary, sizeof(ev->summary), safe_text(summary, ""));
            ev->monotonic_ns = mono;
        }
        return;
    }

    append_event_internal(event_type, summary, mono);
}

bool audit_persist_hardware_snapshot(const vita_hw_snapshot_t *snapshot) {
    const char *sql =
        "INSERT INTO hardware_snapshot "
        "(boot_id,cpu_arch,cpu_model,ram_bytes,firmware_type,console_type,net_count,storage_count,usb_count,wifi_count,detected_at_unix) "
        "VALUES (?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,?11);";
    sqlite3_stmt *st = NULL;
    bool ok;

    if (!snapshot || !g_audit.persistent_ready || !g_audit.db) {
        return false;
    }

    if (sqlite3_prepare_v2(g_audit.db, sql, -1, &st, NULL) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(st, 1, g_audit.boot_id, -1, SQLITE_STATIC);
    bind_text_or_null(st, 2, snapshot->cpu_arch);
    bind_text_or_null(st, 3, snapshot->cpu_model);
    sqlite3_bind_int64(st, 4, (sqlite3_int64)snapshot->ram_bytes);
    bind_text_or_null(st, 5, snapshot->firmware_type);
    bind_text_or_null(st, 6, snapshot->console_type);
    sqlite3_bind_int(st, 7, snapshot->net_count);
    sqlite3_bind_int(st, 8, snapshot->storage_count);
    sqlite3_bind_int(st, 9, snapshot->usb_count);
    sqlite3_bind_int(st, 10, snapshot->wifi_count);
    sqlite3_bind_int64(st, 11, (sqlite3_int64)snapshot->detected_at_unix);

    ok = sqlite3_step(st) == SQLITE_DONE;
    sqlite3_finalize(st);
    return ok;
}

bool audit_persist_ai_proposal(const vita_ai_proposal_t *proposal) {
    const char *sql =
        "INSERT OR REPLACE INTO ai_proposal "
        "(proposal_id,boot_id,created_unix,proposal_type,summary,rationale,risk_level,benefit_level,requires_human_confirmation,status) "
        "VALUES (?1,?2,?3,?4,?5,?6,?7,?8,?9,?10);";
    sqlite3_stmt *st = NULL;
    bool ok;

    if (!proposal || !proposal->proposal_id || !g_audit.persistent_ready || !g_audit.db) {
        return false;
    }

    if (sqlite3_prepare_v2(g_audit.db, sql, -1, &st, NULL) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(st, 1, proposal->proposal_id, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, g_audit.boot_id, -1, SQLITE_STATIC);
    sqlite3_bind_int64(st, 3, (sqlite3_int64)unix_now());
    sqlite3_bind_text(st, 4, safe_text(proposal->proposal_type, "unspecified"), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 5, safe_text(proposal->summary, ""), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 6, safe_text(proposal->rationale, ""), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 7, safe_text(proposal->risk_level, "unknown"), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 8, safe_text(proposal->benefit_level, "unknown"), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(st, 9, proposal->requires_human_confirmation ? 1 : 0);
    sqlite3_bind_text(st, 10, safe_text(proposal->status, "PENDING"), -1, SQLITE_TRANSIENT);

    ok = sqlite3_step(st) == SQLITE_DONE;
    sqlite3_finalize(st);
    return ok;
}

bool audit_persist_human_response(const char *proposal_id, const char *response) {
    const char *sql =
        "INSERT INTO human_response "
        "(proposal_id,boot_id,response,operator_key,response_unix) "
        "VALUES (?1,?2,?3,'s/n',?4);";
    sqlite3_stmt *st = NULL;
    bool ok;

    if (!proposal_id || !response || !g_audit.persistent_ready || !g_audit.db) {
        return false;
    }

    if (sqlite3_prepare_v2(g_audit.db, sql, -1, &st, NULL) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(st, 1, proposal_id, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, g_audit.boot_id, -1, SQLITE_STATIC);
    sqlite3_bind_text(st, 3, response, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(st, 4, (sqlite3_int64)unix_now());

    ok = sqlite3_step(st) == SQLITE_DONE;
    sqlite3_finalize(st);
    return ok;
}

bool audit_upsert_node_peer(const vita_node_peer_t *peer) {
    const char *sql =
        "INSERT OR REPLACE INTO node_peer "
        "(peer_id,first_seen_unix,last_seen_unix,transport,capabilities_json,trust_state,link_state) "
        "VALUES (?1,?2,?3,?4,?5,?6,?7);";
    sqlite3_stmt *st = NULL;
    bool ok;

    if (!peer || !peer->peer_id[0] || !g_audit.persistent_ready || !g_audit.db) {
        return false;
    }

    if (sqlite3_prepare_v2(g_audit.db, sql, -1, &st, NULL) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(st, 1, peer->peer_id, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(st, 2, (sqlite3_int64)peer->first_seen_unix);
    sqlite3_bind_int64(st, 3, (sqlite3_int64)peer->last_seen_unix);
    sqlite3_bind_text(st, 4, safe_text(peer->transport, "unknown"), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 5, safe_text(peer->capabilities_json, "{}"), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 6, safe_text(peer->trust_state, "observed"), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 7, safe_text(peer->link_state, "discovered"), -1, SQLITE_TRANSIENT);

    ok = sqlite3_step(st) == SQLITE_DONE;
    sqlite3_finalize(st);
    return ok;
}

bool audit_export_recent_event_block(char *out, unsigned long out_cap) {
    const char *sql =
        "SELECT event_seq,event_type,event_hash "
        "FROM audit_event "
        "ORDER BY id DESC "
        "LIMIT ?1;";
    sqlite3_stmt *st = NULL;
    char line[192];
    bool first = true;
    int rc;

    if (!out || out_cap < 2 || !g_audit.persistent_ready || !g_audit.db) {
        return false;
    }

    out[0] = '\0';

    if (sqlite3_prepare_v2(g_audit.db, sql, -1, &st, NULL) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int(st, 1, AUDIT_RECENT_BLOCK_EVENTS);

    while ((rc = sqlite3_step(st)) == SQLITE_ROW) {
        sqlite3_int64 seq = sqlite3_column_int64(st, 0);
        const unsigned char *etype = sqlite3_column_text(st, 1);
        const unsigned char *ehash = sqlite3_column_text(st, 2);
        int written;

        written = snprintf(line,
                           sizeof(line),
                           "%s%lld:%s:%s",
                           first ? "" : ";",
                           (long long)seq,
                           etype ? (const char *)etype : "",
                           ehash ? (const char *)ehash : "");
        if (written < 0 || (unsigned long)written >= sizeof(line)) {
            sqlite3_finalize(st);
            return false;
        }

        if (strlen(out) + strlen(line) + 1 >= out_cap) {
            sqlite3_finalize(st);
            return out[0] != '\0';
        }

        strcat(out, line);
        first = false;
    }

    sqlite3_finalize(st);
    return out[0] != '\0';
}

bool audit_init_persistent_backend(const vita_handoff_t *handoff) {
    const char *db_path;
    uint32_t buffered_count;
    uint32_t i;

    db_path = (handoff && handoff->audit_db_path)
        ? handoff->audit_db_path
        : "build/audit/vitaos-audit.db";

    if (sqlite3_open(db_path, &g_audit.db) != SQLITE_OK) {
        if (g_audit.db) {
            sqlite3_close(g_audit.db);
            g_audit.db = NULL;
        }
        return false;
    }

    if (!exec_sql_file(g_audit.db, "schema/audit.sql")) {
        sqlite3_close(g_audit.db);
        g_audit.db = NULL;
        return false;
    }

    g_audit.boot_started_unix = unix_now();
    snprintf(g_audit.boot_id, sizeof(g_audit.boot_id), "boot-%llu", (unsigned long long)g_audit.boot_started_unix);
    copy_text(g_audit.node_id, sizeof(g_audit.node_id), "node-local");
    copy_text(g_audit.arch_name, sizeof(g_audit.arch_name), safe_text(handoff ? handoff->arch_name : NULL, "x86_64"));

    if (!insert_boot_session()) {
        sqlite3_close(g_audit.db);
        g_audit.db = NULL;
        return false;
    }

    g_audit.persistent_ready = true;

    buffered_count = g_audit.early_count;
    for (i = 0; i < buffered_count; ++i) {
        append_event_internal(g_audit.early_events[i].event_type,
                              g_audit.early_events[i].summary,
                              g_audit.early_events[i].monotonic_ns);
    }
    g_audit.early_count = 0;

    return true;
}

#else

void audit_early_buffer_init(void) {
}

bool audit_init_persistent_backend(const vita_handoff_t *handoff) {
    (void)handoff;
    return false;
}

void audit_emit_boot_event(const char *event_type, const char *summary) {
    (void)event_type;
    (void)summary;
}

bool audit_persist_hardware_snapshot(const vita_hw_snapshot_t *snapshot) {
    (void)snapshot;
    return false;
}

bool audit_persist_ai_proposal(const vita_ai_proposal_t *proposal) {
    (void)proposal;
    return false;
}

bool audit_persist_human_response(const char *proposal_id, const char *response) {
    (void)proposal_id;
    (void)response;
    return false;
}

bool audit_upsert_node_peer(const vita_node_peer_t *peer) {
    (void)peer;
    return false;
}

bool audit_export_recent_event_block(char *out, unsigned long out_cap) {
    if (out && out_cap > 0) {
        out[0] = '\0';
    }
    return false;
}

#endif
