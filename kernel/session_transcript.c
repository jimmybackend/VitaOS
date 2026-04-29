#include <vita/session_transcript.h>
#include <vita/audit.h>
#include <vita/hw.h>
#include <vita/storage.h>
#include <vita/session_journal.h>

typedef struct {
    char boot_id[64];
    char node_id[64];
    char host_id[64];
    char arch[32];
    char firmware[16];
    char boot_mode[16];
    char cpu_model[128];
    uint64_t ram_bytes;
    char storage_backend[32];
    char storage_state[32];
} vita_session_identity_t;

typedef struct {
    bool initialized;
    bool active;
    bool busy;
    bool error_reported;
    unsigned int seq;
    char session_id[64];
    char txt_path[VITA_STORAGE_PATH_MAX];
    char jsonl_path[VITA_STORAGE_PATH_MAX];
    char txt[8192];
    char jsonl[8192];
    char state[32];
    char last_text[160];
    vita_session_identity_t identity;
    unsigned long txt_len;
    unsigned long jsonl_len;
} st_t;

static st_t g;

static unsigned long sl(const char *s) {
    unsigned long n = 0;
    while (s && s[n]) n++;
    return n;
}
static void cp(char *d, unsigned long c, const char *s) {
    unsigned long i = 0;
    if (!d || !c) return;
    if (!s) { d[0] = 0; return; }
    while (s[i] && i + 1 < c) { d[i] = s[i]; i++; }
    d[i] = 0;
}
static bool eq(const char *a, const char *b) {
    unsigned long i = 0;
    if (!a || !b) return false;
    while (a[i] && b[i]) { if (a[i] != b[i]) return false; i++; }
    return a[i] == b[i];
}
static void ap(char *d, unsigned long c, unsigned long *len, const char *s) {
    unsigned long i = 0;
    if (!d || !len || !s) return;
    while (s[i] && *len + 1 < c) { d[*len] = s[i++]; (*len)++; }
    d[*len] = 0;
}
static void apc(char *d, unsigned long c, const char *s) {
    unsigned long l = sl(d);
    ap(d, c, &l, s);
}
static void u6(unsigned int v, char o[7]) {
    o[0] = '0' + (v / 100000) % 10;
    o[1] = '0' + (v / 10000) % 10;
    o[2] = '0' + (v / 1000) % 10;
    o[3] = '0' + (v / 100) % 10;
    o[4] = '0' + (v / 10) % 10;
    o[5] = '0' + v % 10;
    o[6] = 0;
}
static void u64_to_dec(uint64_t value, char out[32]) {
    char tmp[32];
    int i = 0;
    int j = 0;
    if (!out) return;
    if (value == 0) { out[0] = '0'; out[1] = 0; return; }
    while (value > 0 && i < (int)(sizeof(tmp) - 1)) { tmp[i++] = (char)('0' + (value % 10ULL)); value /= 10ULL; }
    while (i > 0) out[j++] = tmp[--i];
    out[j] = 0;
}

static void mark_transcript_error_once(const char *reason) {
    if (g.error_reported) return;
    g.error_reported = true;
    session_journal_log_system("transcript_error", reason);
}
static bool flushv(void) {
    static char a[VITA_STORAGE_READ_MAX], b[VITA_STORAGE_READ_MAX];
    if (!storage_write_text(g.txt_path, g.txt) || !storage_write_text(g.jsonl_path, g.jsonl))
        return false;
    if (g.txt_len >= (VITA_STORAGE_READ_MAX - 1U) || g.jsonl_len >= (VITA_STORAGE_READ_MAX - 1U))
        return true;
    return storage_read_text(g.txt_path, a, sizeof(a)) &&
           storage_read_text(g.jsonl_path, b, sizeof(b)) &&
           eq(a, g.txt) && eq(b, g.jsonl);
}
/*
 * Critical transcript safety notes:
 * - Never call console_write_line()/console_write_raw() from transcript code.
 *   Doing so can create recursive logging loops through system output paths.
 * - g.busy is a hard reentry guard that blocks recursion while an event is being appended/flushed.
 * - error_reported suppresses repeated transcript_error audit spam after the first hard failure.
 * - If write/read/compare persistence verification fails, transcript enters error/inactive state.
 */
static void event(const char *t, const char *a, const char *x) {
    char seq[7], line[256];
    unsigned long n = 0, m = 0;
    if (!g.active || g.busy) return;
    g.busy = true;
    g.seq++;
    u6(g.seq, seq);
    cp(line, sizeof(line), "[");
    apc(line, sizeof(line), seq);
    apc(line, sizeof(line), "] ");
    apc(line, sizeof(line), t);
    apc(line, sizeof(line), ": ");
    apc(line, sizeof(line), x ? x : "");
    ap(g.txt, sizeof(g.txt), &g.txt_len, line);
    ap(g.txt, sizeof(g.txt), &g.txt_len, "\n");
    ap(g.jsonl, sizeof(g.jsonl), &g.jsonl_len, "{\"session_id\":\"");
    ap(g.jsonl, sizeof(g.jsonl), &g.jsonl_len, g.session_id);
    ap(g.jsonl, sizeof(g.jsonl), &g.jsonl_len, "\",\"boot_id\":\"");
    ap(g.jsonl, sizeof(g.jsonl), &g.jsonl_len, g.identity.boot_id);
    ap(g.jsonl, sizeof(g.jsonl), &g.jsonl_len, "\",\"node_id\":\"");
    ap(g.jsonl, sizeof(g.jsonl), &g.jsonl_len, g.identity.node_id);
    ap(g.jsonl, sizeof(g.jsonl), &g.jsonl_len, "\",\"host_id\":\"");
    ap(g.jsonl, sizeof(g.jsonl), &g.jsonl_len, g.identity.host_id);
    ap(g.jsonl, sizeof(g.jsonl), &g.jsonl_len, "\",\"firmware\":\"");
    ap(g.jsonl, sizeof(g.jsonl), &g.jsonl_len, g.identity.firmware);
    ap(g.jsonl, sizeof(g.jsonl), &g.jsonl_len, "\",\"arch\":\"");
    ap(g.jsonl, sizeof(g.jsonl), &g.jsonl_len, g.identity.arch);
    ap(g.jsonl, sizeof(g.jsonl), &g.jsonl_len, "\",\"boot_mode\":\"");
    ap(g.jsonl, sizeof(g.jsonl), &g.jsonl_len, g.identity.boot_mode);
    ap(g.jsonl, sizeof(g.jsonl), &g.jsonl_len, "\",\"cpu_model\":\"");
    ap(g.jsonl, sizeof(g.jsonl), &g.jsonl_len, g.identity.cpu_model);
    ap(g.jsonl, sizeof(g.jsonl), &g.jsonl_len, "\",\"ram_bytes\":");
    {
        char ram[32];
        u64_to_dec(g.identity.ram_bytes, ram);
        ap(g.jsonl, sizeof(g.jsonl), &g.jsonl_len, ram);
    }
    ap(g.jsonl, sizeof(g.jsonl), &g.jsonl_len, ",\"storage_backend\":\"");
    ap(g.jsonl, sizeof(g.jsonl), &g.jsonl_len, g.identity.storage_backend);
    ap(g.jsonl, sizeof(g.jsonl), &g.jsonl_len, "\",\"storage_state\":\"");
    ap(g.jsonl, sizeof(g.jsonl), &g.jsonl_len, g.identity.storage_state);
    ap(g.jsonl, sizeof(g.jsonl), &g.jsonl_len, "\",\"event_type\":\"");
    ap(g.jsonl, sizeof(g.jsonl), &g.jsonl_len, t);
    ap(g.jsonl, sizeof(g.jsonl), &g.jsonl_len, "\",\"actor\":\"");
    ap(g.jsonl, sizeof(g.jsonl), &g.jsonl_len, a);
    ap(g.jsonl, sizeof(g.jsonl), &g.jsonl_len, "\",\"text\":\"");
    if (x) {
        while (x[n] && g.jsonl_len + 1 < sizeof(g.jsonl)) {
            char c = x[n++];
            if (c == '"') ap(g.jsonl, sizeof(g.jsonl), &g.jsonl_len, "\\\"");
            else { char one[2] = {c, 0}; ap(g.jsonl, sizeof(g.jsonl), &g.jsonl_len, one); }
        }
    }
    ap(g.jsonl, sizeof(g.jsonl), &g.jsonl_len, "\",\"seq\":");
    while (seq[m] == '0' && seq[m + 1]) m++;
    ap(g.jsonl, sizeof(g.jsonl), &g.jsonl_len, seq + m);
    ap(g.jsonl, sizeof(g.jsonl), &g.jsonl_len, "}\n");
    if (!flushv()) {
        g.active = false;
        cp(g.state, sizeof(g.state), "error");
        mark_transcript_error_once("write_read_compare_failed");
    }
    g.busy = false;
}

bool session_transcript_init(const vita_handoff_t *h, const char *arch) {
    unsigned int i;
    char p[VITA_STORAGE_PATH_MAX], num[7], scratch[8];
    cp(g.session_id, sizeof(g.session_id), "");
    cp(g.txt_path, sizeof(g.txt_path), "");
    cp(g.jsonl_path, sizeof(g.jsonl_path), "");
    cp(g.txt, sizeof(g.txt), "");
    cp(g.jsonl, sizeof(g.jsonl), "");
    cp(g.state, sizeof(g.state), "");
    cp(g.last_text, sizeof(g.last_text), "");
    g.initialized = false;
    g.active = false;
    g.busy = false;
    g.error_reported = false;
    g.seq = 0;
    g.txt_len = 0;
    g.jsonl_len = 0;
    g.initialized = true;
    cp(g.state, sizeof(g.state), "degraded");
    cp(g.identity.arch, sizeof(g.identity.arch), arch && arch[0] ? arch : "unknown");
    cp(g.identity.firmware, sizeof(g.identity.firmware), h && h->firmware_type == VITA_FIRMWARE_HOSTED ? "hosted" : (h && h->firmware_type == VITA_FIRMWARE_UEFI ? "uefi" : "unknown"));
    cp(g.identity.boot_mode, sizeof(g.identity.boot_mode), g.identity.firmware);
    cp(g.identity.boot_id, sizeof(g.identity.boot_id), "unknown");
    cp(g.identity.node_id, sizeof(g.identity.node_id), "unknown");
    cp(g.identity.host_id, sizeof(g.identity.host_id), "unknown");
    cp(g.identity.cpu_model, sizeof(g.identity.cpu_model), "unknown");
    cp(g.identity.storage_backend, sizeof(g.identity.storage_backend), "unknown");
    cp(g.identity.storage_state, sizeof(g.identity.storage_state), "unknown");
    g.identity.ram_bytes = 0;
    {
        vita_hw_snapshot_t hw;
        vita_storage_status_t st;
        char boot_id[64];
        char node_id[64];
        if (audit_get_identity(boot_id, sizeof(boot_id), node_id, sizeof(node_id))) {
            cp(g.identity.boot_id, sizeof(g.identity.boot_id), boot_id);
            cp(g.identity.node_id, sizeof(g.identity.node_id), node_id);
        } /* TODO: persist host/node identity for UEFI-only flows without SQLite. */
        if (hw_discovery_run(h, &hw)) {
            cp(g.identity.cpu_model, sizeof(g.identity.cpu_model), hw.cpu_model[0] ? hw.cpu_model : "unknown");
            g.identity.ram_bytes = hw.ram_bytes;
        }
        storage_get_status(&st);
        cp(g.identity.storage_backend, sizeof(g.identity.storage_backend), st.backend_name[0] ? st.backend_name : "unknown");
        cp(g.identity.storage_state, sizeof(g.identity.storage_state), st.bootstrap_verified ? "verified" : "degraded");
    }
    if (!storage_is_bootstrap_verified()) {
        mark_transcript_error_once("storage_not_verified");
        return false;
    }
    for (i = 1; i < 999999; i++) {
        u6(i, num);
        cp(p, sizeof(p), "/vita/audit/sessions/session-");
        apc(p, sizeof(p), num);
        apc(p, sizeof(p), ".txt");
        if (!storage_read_text(p, scratch, sizeof(scratch))) {
            cp(g.session_id, sizeof(g.session_id), "session-");
            apc(g.session_id, sizeof(g.session_id), num);
            break;
        }
    }
    if (!g.session_id[0]) cp(g.session_id, sizeof(g.session_id), "session-boot-unknown");
    cp(g.txt_path, sizeof(g.txt_path), "/vita/audit/sessions/");
    apc(g.txt_path, sizeof(g.txt_path), g.session_id);
    apc(g.txt_path, sizeof(g.txt_path), ".txt");
    cp(g.jsonl_path, sizeof(g.jsonl_path), "/vita/audit/sessions/");
    apc(g.jsonl_path, sizeof(g.jsonl_path), g.session_id);
    apc(g.jsonl_path, sizeof(g.jsonl_path), ".jsonl");
    ap(g.txt, sizeof(g.txt), &g.txt_len, "=== VitaOS session transcript ===\nsession_id: ");
    ap(g.txt, sizeof(g.txt), &g.txt_len, g.session_id);
    ap(g.txt, sizeof(g.txt), &g.txt_len, "\n\nDatos de esta sesion:\nsession_id: ");
    ap(g.txt, sizeof(g.txt), &g.txt_len, g.session_id);
    ap(g.txt, sizeof(g.txt), &g.txt_len, "\nboot_id: ");
    ap(g.txt, sizeof(g.txt), &g.txt_len, g.identity.boot_id);
    ap(g.txt, sizeof(g.txt), &g.txt_len, "\nnode_id: ");
    ap(g.txt, sizeof(g.txt), &g.txt_len, g.identity.node_id);
    ap(g.txt, sizeof(g.txt), &g.txt_len, "\nhost_id: ");
    ap(g.txt, sizeof(g.txt), &g.txt_len, g.identity.host_id);
    ap(g.txt, sizeof(g.txt), &g.txt_len, "\narquitectura: ");
    ap(g.txt, sizeof(g.txt), &g.txt_len, g.identity.arch);
    ap(g.txt, sizeof(g.txt), &g.txt_len, "\nfirmware: ");
    ap(g.txt, sizeof(g.txt), &g.txt_len, g.identity.firmware);
    ap(g.txt, sizeof(g.txt), &g.txt_len, "\nmodo de arranque: ");
    ap(g.txt, sizeof(g.txt), &g.txt_len, g.identity.boot_mode);
    ap(g.txt, sizeof(g.txt), &g.txt_len, "\nCPU: ");
    ap(g.txt, sizeof(g.txt), &g.txt_len, g.identity.cpu_model);
    ap(g.txt, sizeof(g.txt), &g.txt_len, "\nRAM: ");
    {
        char ram[32];
        u64_to_dec(g.identity.ram_bytes, ram);
        ap(g.txt, sizeof(g.txt), &g.txt_len, ram);
    }
    ap(g.txt, sizeof(g.txt), &g.txt_len, "\nstorage/backend: ");
    ap(g.txt, sizeof(g.txt), &g.txt_len, g.identity.storage_backend);
    ap(g.txt, sizeof(g.txt), &g.txt_len, "\nstorage_state: ");
    ap(g.txt, sizeof(g.txt), &g.txt_len, g.identity.storage_state);
    ap(g.txt, sizeof(g.txt), &g.txt_len, "\n\n");
    g.active = true;
    cp(g.state, sizeof(g.state), "active");
    event("session_start", "system", "transcript started");
    return g.active;
}
void session_transcript_shutdown(void) {
    if (g.active) event("session_end", "system", "session closed");
}
bool session_transcript_is_active(void) {
    return g.initialized && g.active;
}
void session_transcript_log_user_input(const char *t) {
    event("user_input", "user", t ? t : "");
}
void session_transcript_log_command_executed(const char *t) {
    event("command_executed", "user", t ? t : "");
}
void session_transcript_log_editor_event(const char *e, const char *t) {
    event(e ? e : "editor_event", "editor", t ? t : "");
}
void session_transcript_log_storage_status(const char *t) {
    event("storage_status", "system", t ? t : "");
}
void session_transcript_log_journal_status(const char *t) {
    event("journal_status", "system", t ? t : "");
}
void session_transcript_log_system_output(const char *t, bool raw) {
    if (!t || !t[0]) return;
    if (raw && eq(g.last_text, t)) return;
    cp(g.last_text, sizeof(g.last_text), t);
    event("system_output", "system", t);
}
const char *session_transcript_txt_path(void) {
    return g.txt_path[0] ? g.txt_path : "unknown";
}
const char *session_transcript_jsonl_path(void) {
    return g.jsonl_path[0] ? g.jsonl_path : "unknown";
}
const char *session_transcript_state(void) { return g.state; }
