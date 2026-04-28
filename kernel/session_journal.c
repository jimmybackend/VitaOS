/*
 * kernel/session_journal.c
 * Persistent visible command/session journal for the F1A/F1B USB workflow.
 *
 * This is not a hidden keylogger. It records submitted commands after Enter
 * and summarized system events into /vita/audit/ files when writable storage
 * is available. Sensitive-looking commands are redacted.
 */

#include <stdint.h>

#include <vita/audit.h>
#include <vita/console.h>
#include <vita/session_journal.h>
#include <vita/storage.h>

#define SESSION_JOURNAL_JSONL_PATH "/vita/audit/session-journal.jsonl"
#define SESSION_JOURNAL_TEXT_PATH  "/vita/audit/session-journal.txt"
#define SESSION_JOURNAL_BUFFER_MAX 8192U
#define SESSION_JOURNAL_EVENT_TEXT_MAX 160U

typedef struct {
    bool initialized;
    bool active;
    bool truncated;
    bool last_flush_ok;
    uint32_t seq;
    char jsonl[SESSION_JOURNAL_BUFFER_MAX];
    char text[SESSION_JOURNAL_BUFFER_MAX];
    unsigned long jsonl_len;
    unsigned long text_len;
    char last_error[VITA_STORAGE_ERROR_MAX];
} session_journal_state_t;

static session_journal_state_t g_journal;

static unsigned long str_len(const char *s) {
    unsigned long n = 0;

    if (!s) {
        return 0;
    }

    while (s[n]) {
        n++;
    }

    return n;
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

static bool is_space_char(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static const char *skip_spaces(const char *s) {
    if (!s) {
        return "";
    }

    while (*s && is_space_char(*s)) {
        s++;
    }

    return s;
}

static void str_copy(char *dst, unsigned long cap, const char *src) {
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

static void u32_to_dec(uint32_t value, char out[16]) {
    char tmp[16];
    int i = 0;
    int j = 0;

    if (!out) {
        return;
    }

    if (value == 0U) {
        out[0] = '0';
        out[1] = '\0';
        return;
    }

    while (value > 0U && i < (int)(sizeof(tmp) - 1U)) {
        tmp[i++] = (char)('0' + (value % 10U));
        value /= 10U;
    }

    while (i > 0) {
        out[j++] = tmp[--i];
    }

    out[j] = '\0';
}

static void set_error(const char *text) {
    str_copy(g_journal.last_error, sizeof(g_journal.last_error), text ? text : "unknown");
}

static void append_to_buffer(char *buf,
                             unsigned long cap,
                             unsigned long *len,
                             const char *text) {
    unsigned long i;

    if (!buf || cap == 0 || !len || !text) {
        return;
    }

    for (i = 0; text[i]; ++i) {
        if ((*len + 1U) >= cap) {
            g_journal.truncated = true;
            buf[*len] = '\0';
            return;
        }

        buf[*len] = text[i];
        (*len)++;
    }

    buf[*len] = '\0';
}

static void append_text_line(const char *text) {
    append_to_buffer(g_journal.text,
                     sizeof(g_journal.text),
                     &g_journal.text_len,
                     text ? text : "");
    append_to_buffer(g_journal.text,
                     sizeof(g_journal.text),
                     &g_journal.text_len,
                     "\n");
}

static bool contains_token(const char *text, const char *token) {
    unsigned long i;
    unsigned long j;

    if (!text || !token || !token[0]) {
        return false;
    }

    for (i = 0; text[i]; ++i) {
        j = 0;
        while (text[i + j] && token[j] && text[i + j] == token[j]) {
            j++;
        }
        if (!token[j]) {
            return true;
        }
    }

    return false;
}

static bool looks_sensitive(const char *text) {
    if (!text) {
        return false;
    }

    return contains_token(text, "password") ||
           contains_token(text, "passwd") ||
           contains_token(text, "token") ||
           contains_token(text, "secret") ||
           contains_token(text, "api_key") ||
           contains_token(text, "apikey") ||
           contains_token(text, "key=") ||
           contains_token(text, "clave") ||
           contains_token(text, "contrasena");
}

static void sanitize_text(const char *src,
                          char *dst,
                          unsigned long dst_cap,
                          bool *redacted) {
    unsigned long i = 0;
    unsigned long j = 0;

    if (redacted) {
        *redacted = false;
    }

    if (!dst || dst_cap == 0) {
        return;
    }

    if (!src) {
        dst[0] = '\0';
        return;
    }

    if (looks_sensitive(src)) {
        str_copy(dst, dst_cap, "[redacted sensitive command]");
        if (redacted) {
            *redacted = true;
        }
        return;
    }

    while (src[i] && (j + 1U) < dst_cap) {
        unsigned char c = (unsigned char)src[i++];

        if (c == '\r' || c == '\n' || c == '\t') {
            dst[j++] = ' ';
        } else if (c >= 32U && c < 127U) {
            dst[j++] = (char)c;
        } else {
            dst[j++] = '?';
        }
    }

    dst[j] = '\0';
}

static void append_json_escaped(const char *text) {
    unsigned long i;

    if (!text) {
        return;
    }

    for (i = 0; text[i]; ++i) {
        char c = text[i];

        if (c == '"') {
            append_to_buffer(g_journal.jsonl, sizeof(g_journal.jsonl), &g_journal.jsonl_len, "\\\"");
        } else if (c == '\\') {
            append_to_buffer(g_journal.jsonl, sizeof(g_journal.jsonl), &g_journal.jsonl_len, "\\\\");
        } else if (c == '\n' || c == '\r') {
            append_to_buffer(g_journal.jsonl, sizeof(g_journal.jsonl), &g_journal.jsonl_len, " ");
        } else if ((unsigned char)c < 32U) {
            append_to_buffer(g_journal.jsonl, sizeof(g_journal.jsonl), &g_journal.jsonl_len, "?");
        } else {
            char one[2];
            one[0] = c;
            one[1] = '\0';
            append_to_buffer(g_journal.jsonl, sizeof(g_journal.jsonl), &g_journal.jsonl_len, one);
        }
    }
}

static void append_json_record(const char *type,
                               const char *summary,
                               bool redacted) {
    char seq[16];

    u32_to_dec(g_journal.seq, seq);

    append_to_buffer(g_journal.jsonl, sizeof(g_journal.jsonl), &g_journal.jsonl_len, "{\"seq\":");
    append_to_buffer(g_journal.jsonl, sizeof(g_journal.jsonl), &g_journal.jsonl_len, seq);
    append_to_buffer(g_journal.jsonl, sizeof(g_journal.jsonl), &g_journal.jsonl_len, ",\"type\":\"");
    append_json_escaped(type ? type : "event");
    append_to_buffer(g_journal.jsonl, sizeof(g_journal.jsonl), &g_journal.jsonl_len, "\",\"summary\":\"");
    append_json_escaped(summary ? summary : "");
    append_to_buffer(g_journal.jsonl, sizeof(g_journal.jsonl), &g_journal.jsonl_len, "\",\"redacted\":");
    append_to_buffer(g_journal.jsonl, sizeof(g_journal.jsonl), &g_journal.jsonl_len, redacted ? "true" : "false");
    append_to_buffer(g_journal.jsonl, sizeof(g_journal.jsonl), &g_journal.jsonl_len, "}\n");
}

static void append_text_record(const char *type, const char *summary, bool redacted) {
    char line[224];
    char seq[16];

    u32_to_dec(g_journal.seq, seq);

    /* Build line manually to avoid stdio in UEFI. */
    line[0] = '\0';
    str_copy(line, sizeof(line), "#");
    {
        unsigned long len = str_len(line);
        append_to_buffer(line, sizeof(line), &len, seq);
        append_to_buffer(line, sizeof(line), &len, " ");
        append_to_buffer(line, sizeof(line), &len, type ? type : "event");
        append_to_buffer(line, sizeof(line), &len, ": ");
        append_to_buffer(line, sizeof(line), &len, summary ? summary : "");
        if (redacted) {
            append_to_buffer(line, sizeof(line), &len, " [redacted]");
        }
    }

    append_text_line(line);
}

bool session_journal_flush(void) {
    vita_storage_status_t st;
    bool ok_jsonl;
    bool ok_text;

    if (!g_journal.initialized || !g_journal.active) {
        set_error("journal inactive");
        return false;
    }

    ok_jsonl = storage_write_text(SESSION_JOURNAL_JSONL_PATH, g_journal.jsonl);
    ok_text = storage_write_text(SESSION_JOURNAL_TEXT_PATH, g_journal.text);

    if (ok_jsonl && ok_text) {
        g_journal.last_flush_ok = true;
        set_error("ok");
        return true;
    }

    storage_get_status(&st);
    g_journal.last_flush_ok = false;
    set_error(st.last_error[0] ? st.last_error : "storage write failed");
    return false;
}

static void journal_add_event(const char *type, const char *summary, bool redact_check) {
    char safe[SESSION_JOURNAL_EVENT_TEXT_MAX];
    bool redacted = false;

    if (!g_journal.initialized || !g_journal.active) {
        return;
    }

    sanitize_text(summary, safe, sizeof(safe), &redacted);
    if (!redact_check) {
        redacted = false;
    }

    g_journal.seq++;
    append_json_record(type ? type : "event", safe, redacted);
    append_text_record(type ? type : "event", safe, redacted);

    if (g_journal.truncated) {
        set_error("journal buffer truncated");
    }

    (void)session_journal_flush();
}

bool session_journal_init(void) {
    vita_storage_status_t st;

    g_journal.initialized = true;
    g_journal.active = storage_is_ready();
    g_journal.truncated = false;
    g_journal.last_flush_ok = false;
    g_journal.seq = 0;
    g_journal.jsonl_len = 0;
    g_journal.text_len = 0;
    g_journal.jsonl[0] = '\0';
    g_journal.text[0] = '\0';
    set_error("ok");

    if (!g_journal.active) {
        storage_get_status(&st);
        set_error(st.last_error[0] ? st.last_error : "storage unavailable");
        console_write_line("Persistent journal: unavailable / Bitacora persistente: no disponible");
        console_write_line(g_journal.last_error);
        return false;
    }

    append_text_line("VitaOS persistent session journal / Bitacora persistente de sesion VitaOS");
    append_text_line("Visible audit note: submitted commands and summarized system responses are recorded.");
    append_text_line("Nota visible: se registran comandos enviados y respuestas resumidas del sistema.");
    append_text_line("");

    journal_add_event("journal_started", "persistent session journal active", false);

    console_write_line("Persistent journal: READY / Bitacora persistente: LISTA");
    console_write_line(SESSION_JOURNAL_JSONL_PATH);
    console_write_line(SESSION_JOURNAL_TEXT_PATH);
    audit_emit_boot_event("SESSION_JOURNAL_READY", SESSION_JOURNAL_JSONL_PATH);

    return true;
}

bool session_journal_is_active(void) {
    return g_journal.initialized && g_journal.active;
}

void session_journal_log_command(const char *command) {
    journal_add_event("user_command", command ? command : "", true);
}

void session_journal_log_system(const char *event_type, const char *summary) {
    journal_add_event(event_type ? event_type : "system_response", summary ? summary : "", true);
}

void session_journal_show_status(void) {
    char seq[16];

    u32_to_dec(g_journal.seq, seq);

    console_write_line("Journal / Bitacora:");
    console_write_line(g_journal.initialized ? "initialized: yes" : "initialized: no");
    console_write_line(g_journal.active ? "active: yes" : "active: no");
    console_write_line(g_journal.last_flush_ok ? "last_flush: ok" : "last_flush: not-ok");
    console_write_line(g_journal.truncated ? "truncated: yes" : "truncated: no");
    console_write_line("seq:");
    console_write_line(seq);
    console_write_line("jsonl_path:");
    console_write_line(SESSION_JOURNAL_JSONL_PATH);
    console_write_line("text_path:");
    console_write_line(SESSION_JOURNAL_TEXT_PATH);
    console_write_line("last_error:");
    console_write_line(g_journal.last_error[0] ? g_journal.last_error : "none");
    console_write_line("Privacy / Privacidad:");
    console_write_line("Records submitted commands, not raw hidden keystrokes.");
    console_write_line("Registra comandos enviados, no teclas ocultas en bruto.");
    console_write_line("Sensitive-looking commands are redacted / comandos sensibles se ocultan.");
}

static void journal_print_text(void) {
    unsigned long i = 0;
    char line[160];
    unsigned long j = 0;

    if (!g_journal.text[0]) {
        console_write_line("journal show: empty");
        return;
    }

    while (g_journal.text[i]) {
        char c = g_journal.text[i++];

        if (c == '\n' || (j + 1U) >= sizeof(line)) {
            line[j] = '\0';
            console_write_line(line);
            j = 0;
            if (c != '\n') {
                line[j++] = c;
            }
            continue;
        }

        line[j++] = c;
    }

    if (j > 0) {
        line[j] = '\0';
        console_write_line(line);
    }
}

bool session_journal_handle_command(const char *cmd) {
    const char *args;

    if (!cmd) {
        session_journal_show_status();
        return true;
    }

    if (str_eq(cmd, "journal") || str_eq(cmd, "journal status")) {
        session_journal_show_status();
        return true;
    }

    if (str_eq(cmd, "journal paths")) {
        console_write_line(SESSION_JOURNAL_JSONL_PATH);
        console_write_line(SESSION_JOURNAL_TEXT_PATH);
        return true;
    }

    if (str_eq(cmd, "journal flush")) {
        if (session_journal_flush()) {
            console_write_line("journal flush: ok");
        } else {
            console_write_line("journal flush: failed");
            console_write_line(g_journal.last_error[0] ? g_journal.last_error : "unknown");
        }
        return true;
    }

    if (str_eq(cmd, "journal show")) {
        journal_print_text();
        return true;
    }

    if (starts_with(cmd, "journal note ")) {
        args = skip_spaces(cmd + str_len("journal note "));
        if (!args[0]) {
            console_write_line("usage: journal note text");
            return false;
        }
        session_journal_log_system("operator_note", args);
        console_write_line("journal note: recorded");
        return true;
    }

    if (str_eq(cmd, "journal help")) {
        console_write_line("Journal commands / Comandos de bitacora:");
        console_write_line("journal");
        console_write_line("journal status");
        console_write_line("journal show");
        console_write_line("journal flush");
        console_write_line("journal paths");
        console_write_line("journal note text");
        return true;
    }

    console_write_line("journal: unknown command");
    console_write_line("try: journal help");
    return false;
}
