/*
 * kernel/console.c
 * Early text console bindings + guided console helpers for F1A/F1B.
 */

#include <vita/console.h>

static vita_console_write_fn g_console_writer = 0;
static vita_console_write_raw_fn g_console_raw_writer = 0;
static vita_console_read_line_fn g_console_reader = 0;
static vita_console_clear_fn g_console_clear = 0;
static bool g_console_ansi_enabled = false;

static bool g_pager_enabled = false;
static bool g_pager_busy = false;
static unsigned long g_pager_line_count = 0;
static unsigned long g_pager_line_limit = VITA_CONSOLE_PAGE_LINES_DEFAULT;

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

static void str_append(char *dst, unsigned long cap, const char *src) {
    unsigned long i = 0;
    unsigned long j = 0;

    if (!dst || cap == 0 || !src) {
        return;
    }

    while (i < cap && dst[i]) {
        i++;
    }

    if (i >= cap) {
        return;
    }

    while (src[j] && (i + 1) < cap) {
        dst[i++] = src[j++];
    }

    dst[i] = '\0';
}

static void u32_to_dec(uint32_t value, char out[16]) {
    char tmp[16];
    int i = 0;
    int j = 0;

    if (value == 0) {
        out[0] = '0';
        out[1] = '\0';
        return;
    }

    while (value > 0 && i < (int)(sizeof(tmp) - 1)) {
        tmp[i++] = (char)('0' + (value % 10U));
        value /= 10U;
    }

    while (i > 0) {
        out[j++] = tmp[--i];
    }

    out[j] = '\0';
}

static const char *safe_text(const char *text, const char *fallback) {
    if (text && text[0]) {
        return text;
    }
    return fallback;
}

static const char *mode_name(vita_console_mode_t mode) {
    switch (mode) {
        case VITA_CONSOLE_MODE_BOOT:
            return "boot";
        case VITA_CONSOLE_MODE_GUIDED:
            return "guided";
        case VITA_CONSOLE_MODE_TECHNICAL:
            return "technical";
        case VITA_CONSOLE_MODE_EMERGENCY:
            return "emergency";
        case VITA_CONSOLE_MODE_AUDIT:
            return "audit";
        case VITA_CONSOLE_MODE_NODES:
            return "nodes";
        default:
            return "unknown";
    }
}

static void console_write_kv(const char *key, const char *value) {
    char line[192];

    str_copy(line, sizeof(line), safe_text(key, ""));
    str_append(line, sizeof(line), safe_text(value, ""));
    console_write_line(line);
}

static void console_write_kv_u32(const char *key, uint32_t value) {
    char num[16];
    u32_to_dec(value, num);
    console_write_kv(key, num);
}

static void console_pager_after_line(void) {
    char wait_line[8];

    if (!g_pager_enabled || g_pager_busy || !g_console_reader) {
        return;
    }

    g_pager_line_count++;
    if (g_pager_line_count < g_pager_line_limit) {
        return;
    }

    g_pager_busy = true;
    console_write_raw("-- more / mas: Enter --");
    (void)console_read_line(wait_line, sizeof(wait_line));
    g_pager_line_count = 0;
    g_pager_busy = false;
}

static bool style_is_color(vita_console_style_t style) {
    return style == VITA_CONSOLE_STYLE_ES ||
           style == VITA_CONSOLE_STYLE_ERROR ||
           style == VITA_CONSOLE_STYLE_OK ||
           style == VITA_CONSOLE_STYLE_WARNING;
}

static const char *style_ansi(vita_console_style_t style) {
    switch (style) {
        case VITA_CONSOLE_STYLE_ES:
        case VITA_CONSOLE_STYLE_OK:
            return "\033[32m";
        case VITA_CONSOLE_STYLE_ERROR:
            return "\033[31m";
        case VITA_CONSOLE_STYLE_WARNING:
            return "\033[33m";
        default:
            return "\033[0m";
    }
}

void console_bind_writer(vita_console_write_fn writer) {
    g_console_writer = writer;
}

void console_bind_raw_writer(vita_console_write_raw_fn writer) {
    g_console_raw_writer = writer;
}

void console_bind_reader(vita_console_read_line_fn reader) {
    g_console_reader = reader;
}

void console_bind_clear(vita_console_clear_fn clear_fn) {
    g_console_clear = clear_fn;
}

void console_early_init(void) {
    /* Early console writer/reader are provided by architecture boot stage. */
#ifdef VITA_HOSTED
    g_console_ansi_enabled = true;
#else
    g_console_ansi_enabled = false;
#endif
}

void console_write_raw(const char *text) {
    if (!text) {
        return;
    }

    if (g_console_raw_writer) {
        g_console_raw_writer(text);
        return;
    }

    if (g_console_writer) {
        g_console_writer(text);
    }
}

void console_write_line(const char *text) {
    if (!g_console_writer || !text) {
        return;
    }
    g_console_writer(text);
    console_pager_after_line();
}

void console_set_style(vita_console_style_t style) {
    if (!g_console_ansi_enabled || !style_is_color(style)) {
        return;
    }
    console_write_raw(style_ansi(style));
}

void console_reset_style(void) {
    if (!g_console_ansi_enabled) {
        return;
    }
    console_write_raw("\033[0m");
}

static void console_write_styled(vita_console_style_t style, const char *prefix, const char *text) {
    char line[256];

    if (prefix && prefix[0]) {
        str_copy(line, sizeof(line), prefix);
        str_append(line, sizeof(line), text ? text : "");
    } else {
        str_copy(line, sizeof(line), text ? text : "");
    }

    console_set_style(style);
    console_write_line(line);
    console_reset_style();
}

void console_write_en(const char *text) { console_write_styled(VITA_CONSOLE_STYLE_EN, "", text); }
void console_write_es(const char *text) { console_write_styled(VITA_CONSOLE_STYLE_ES, "", text); }
void console_write_error(const char *text) {
    console_write_styled(VITA_CONSOLE_STYLE_ERROR, g_console_ansi_enabled ? "" : "[ERROR] ", text);
}
void console_write_ok(const char *text) {
    console_write_styled(VITA_CONSOLE_STYLE_OK, g_console_ansi_enabled ? "" : "[OK] ", text);
}
void console_write_warning(const char *text) {
    console_write_styled(VITA_CONSOLE_STYLE_WARNING, g_console_ansi_enabled ? "" : "[WARN] ", text);
}

bool console_read_line(char *out, unsigned long out_cap) {
    if (!out || out_cap == 0) {
        return false;
    }

    out[0] = '\0';

    if (!g_console_reader) {
        console_input_style_end();
        console_prompt_end();
        return false;
    }

    {
        bool ok = g_console_reader(out, out_cap);
        console_input_style_end();
        console_prompt_end();
        return ok;
    }
}

void console_clear_screen(void) {
    if (g_console_clear) {
        g_console_clear();
        g_pager_line_count = 0;
        return;
    }

    console_write_line("");
    console_write_line("----------------------------------------");
}

void console_pager_begin(unsigned long page_lines) {
    g_pager_enabled = true;
    g_pager_busy = false;
    g_pager_line_count = 0;
    g_pager_line_limit = page_lines ? page_lines : VITA_CONSOLE_PAGE_LINES_DEFAULT;
}

void console_pager_end(void) {
    g_pager_enabled = false;
    g_pager_busy = false;
    g_pager_line_count = 0;
}

void console_banner(const vita_boot_status_t *status) {
    console_write_en("VitaOS with AI /");
    console_write_es("VitaOS con IA");
    console_write_ok("Boot: OK");

    if (status && status->arch_name && status->arch_name[0]) {
        char line[96];
        str_copy(line, sizeof(line), "Arch: ");
        str_append(line, sizeof(line), status->arch_name);
        console_write_line(line);
    } else {
        console_write_warning("Arch: unknown");
    }

    if (status && status->console_ready) { console_write_ok("Console: OK"); } else { console_write_warning("Console: MISSING"); }
    if (status && status->audit_ready) { console_write_ok("Audit: READY"); } else { console_write_error("Audit: FAILED"); }
}

void console_guided_show_welcome(const vita_console_state_t *state) {
    console_write_line("VitaOS with AI / VitaOS con IA");
    console_write_line("");
    console_write_line("Status / Estado:");
    console_write_line("- Boot: OK");
    console_write_line((state && state->audit_ready)
        ? "- Audit SQLite: READY / Auditoria SQLite: LISTA"
        : "- Audit SQLite: FAILED / Auditoria SQLite: FALLA");
    console_write_line((state && state->console_ready)
        ? "- Guided console: READY / Consola guiada: LISTA"
        : "- Guided console: OFFLINE / Consola guiada: FUERA DE LINEA");
    console_write_line((state && state->proposal_engine_ready)
        ? "- Proposal engine: READY / Motor de propuestas: LISTO"
        : "- Proposal engine: OFFLINE / Motor de propuestas: FUERA DE LINEA");
    console_write_line((state && state->node_core_ready)
        ? "- VitaNet hosted: READY / VitaNet hosted: LISTO"
        : "- VitaNet hosted: LIMITED / VitaNet hosted: LIMITADO");

    if (state) {
        console_write_kv("Arch / Arquitectura: ", safe_text(state->arch_name, "unknown"));
        console_write_kv("Mode / Modo: ", mode_name(state->mode));
        console_write_kv("Boot mode / Modo de arranque: ", safe_text(state->boot_mode, "unknown"));
        console_write_kv("Language / Idioma: ", safe_text(state->language_mode, "es,en"));
        console_write_kv_u32("Peers / Nodos detectados: ", state->peer_count);
        console_write_kv_u32("Pending proposals / Propuestas pendientes: ", state->pending_proposal_count);
    }

    console_write_line("");
    console_write_line("I can help you with / Puedo ayudarte con:");
    console_write_line("1. Emergency assistance / Asistencia de emergencia");
    console_write_line("2. Hardware and system status / Estado de hardware y sistema");
    console_write_line("3. Cooperative nodes / Nodos cooperativos");
    console_write_line("4. Audit review / Revision de auditoria");
    console_write_line("5. Technical console / Consola tecnica");
    console_write_line("");
    console_write_line("Describe what happened or choose an option.");
    console_write_line("Describe que paso o elige una opcion.");
}

void console_guided_show_menu(void) {
    console_write_line("Menu / Menu:");
    console_write_line("- status");
    console_write_line("- hw");
    console_write_line("- audit");
    console_write_line("- peers");
    console_write_line("- proposals");
    console_write_line("- emergency");
    console_write_line("- helpme");
    console_write_line("- clear");
    console_write_line("- approve <id>");
    console_write_line("- reject <id>");
    console_write_line("- shutdown");
}

void console_guided_show_help(void) {
    console_write_line("Help / Ayuda:");
    console_write_line("status      -> show current guided status");
    console_write_line("hw          -> show hardware summary when available");
    console_write_line("audit       -> show audit readiness and restricted-mode note");
    console_write_line("peers       -> show cooperative nodes summary");
    console_write_line("proposals   -> list current system proposals");
    console_write_line("emergency   -> enter emergency-oriented flow");
    console_write_line("helpme      -> show this help and guided menu");
    console_write_line("clear       -> clear screen and redraw guided header");
    console_write_line("approve ID  -> approve a proposal");
    console_write_line("reject ID   -> reject a proposal");
    console_write_line("shutdown    -> stop current command session");
    console_write_line("Keys / Teclas: Up/Down history in UEFI, Enter continues paging.");
}

void console_guided_show_status(const vita_console_state_t *state) {
    console_write_line("Guided status / Estado guiado:");

    if (!state) {
        console_write_line("No state available / No hay estado disponible");
        return;
    }

    console_write_kv("Arch: ", safe_text(state->arch_name, "unknown"));
    console_write_kv("Mode: ", mode_name(state->mode));
    console_write_kv("Boot mode: ", safe_text(state->boot_mode, "unknown"));
    console_write_kv("Language: ", safe_text(state->language_mode, "es,en"));
    console_write_line(state->console_ready ? "Console: OK" : "Console: MISSING");
    console_write_line(state->audit_ready ? "Audit: READY" : "Audit: FAILED");
    console_write_line(state->proposal_engine_ready ? "Proposal engine: READY" : "Proposal engine: OFFLINE");
    console_write_line(state->node_core_ready ? "Node core: READY" : "Node core: LIMITED");
    console_write_kv_u32("Peers discovered: ", state->peer_count);
    console_write_kv_u32("Pending proposals: ", state->pending_proposal_count);
}

void console_prompt_begin(void) {
    if (!g_console_ansi_enabled) {
        return;
    }

    console_write_raw("\033[?25h");
}

void console_prompt_end(void) {
    if (!g_console_ansi_enabled) {
        return;
    }

    console_reset_style();
}

void console_input_style_begin(void) {
    if (!g_console_ansi_enabled) {
        return;
    }

    console_write_raw("\033[31m");
}

void console_input_style_end(void) {
    if (!g_console_ansi_enabled) {
        return;
    }

    console_reset_style();
}

void console_guided_prompt(void) {
    console_prompt_begin();
    console_write_raw("> ");
    console_input_style_begin();
}
