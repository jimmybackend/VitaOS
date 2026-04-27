/*
 * kernel/ai_gateway.c
 * Remote assistant gateway command stub for VitaOS F1A/F1B.
 *
 * Scope:
 * - command/status layer only
 * - stores a target endpoint in memory
 * - produces structured local fallback output
 * - emits audit events for user-visible AI gateway actions
 *
 * Not in this patch:
 * - DHCP/TCP/DNS/TLS
 * - Wi-Fi association
 * - real remote AI calls
 * - dynamic allocation
 */

#include <vita/ai_gateway.h>
#include <vita/audit.h>
#include <vita/console.h>

#define AI_GATEWAY_HOST_MAX 64U

static char g_ai_host[AI_GATEWAY_HOST_MAX];
static unsigned int g_ai_port = 0;
static bool g_ai_endpoint_configured = false;

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

static bool is_space(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static const char *skip_spaces(const char *s) {
    if (!s) {
        return "";
    }

    while (*s && is_space(*s)) {
        s++;
    }

    return s;
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

static void append_text(char *dst, unsigned long cap, const char *src) {
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

static void u32_to_dec(unsigned int value, char out[16]) {
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

static bool parse_uint_port(const char *s, unsigned int *out_port) {
    unsigned int value = 0;
    unsigned long digits = 0;

    if (!s || !out_port) {
        return false;
    }

    s = skip_spaces(s);

    while (*s && !is_space(*s)) {
        if (*s < '0' || *s > '9') {
            return false;
        }

        value = (value * 10U) + (unsigned int)(*s - '0');
        if (value > 65535U) {
            return false;
        }

        digits++;
        s++;
    }

    if (digits == 0 || value == 0U) {
        return false;
    }

    *out_port = value;
    return true;
}

static bool parse_host_port(const char *args, char host[AI_GATEWAY_HOST_MAX], unsigned int *port) {
    unsigned long i = 0;
    const char *p;

    if (!args || !host || !port) {
        return false;
    }

    p = skip_spaces(args);
    if (!p[0]) {
        return false;
    }

    while (p[i] && !is_space(p[i]) && (i + 1U) < AI_GATEWAY_HOST_MAX) {
        host[i] = p[i];
        i++;
    }
    host[i] = '\0';

    if (i == 0 || (p[i] && !is_space(p[i]))) {
        return false;
    }

    p = skip_spaces(p + i);
    return parse_uint_port(p, port);
}

static void write_endpoint_line(void) {
    char line[128];
    char port[16];

    if (!g_ai_endpoint_configured) {
        console_write_line("Endpoint: not configured / no configurado");
        return;
    }

    u32_to_dec(g_ai_port, port);
    copy_text(line, sizeof(line), "Endpoint: ");
    append_text(line, sizeof(line), g_ai_host);
    append_text(line, sizeof(line), ":");
    append_text(line, sizeof(line), port);
    console_write_line(line);
}

void ai_gateway_show_help(void) {
    console_write_line("AI gateway / Puerta de IA remota:");
    console_write_line("ai status                 -> show remote assistant readiness");
    console_write_line("ai connect <host> <port>  -> store target endpoint in memory");
    console_write_line("ai disconnect             -> clear target endpoint");
    console_write_line("ai ask <text>             -> local structured fallback until network transport exists");
    console_write_line("Scope: status/stub only; no DHCP/TCP/DNS/TLS or Wi-Fi auth yet.");
}

void ai_gateway_show_status(void) {
    console_write_line("Remote AI gateway / Puerta de IA remota:");
    write_endpoint_line();
    console_write_line("Transport: not connected / no conectado");
    console_write_line("Network backend: pending patch / backend de red pendiente");
    console_write_line("Security: TLS not implemented yet / TLS no implementado todavia");
    console_write_line("Mode: local audited stub / modo: stub local auditable");
}

static void ai_gateway_connect(const char *args) {
    char host[AI_GATEWAY_HOST_MAX];
    unsigned int port = 0;

    host[0] = '\0';

    if (!parse_host_port(args, host, &port)) {
        console_write_line("Usage / Uso: ai connect <host> <port>");
        console_write_line("Example / Ejemplo: ai connect 192.168.1.10 8080");
        return;
    }

    copy_text(g_ai_host, sizeof(g_ai_host), host);
    g_ai_port = port;
    g_ai_endpoint_configured = true;

    audit_emit_boot_event("AI_GATEWAY_ENDPOINT_SET", g_ai_host);
    console_write_line("AI endpoint stored in memory / Endpoint IA guardado en memoria");
    console_write_line("No network connection was attempted in this patch.");
    write_endpoint_line();
}

static void ai_gateway_disconnect(void) {
    g_ai_host[0] = '\0';
    g_ai_port = 0;
    g_ai_endpoint_configured = false;

    audit_emit_boot_event("AI_GATEWAY_ENDPOINT_CLEARED", "ai disconnect");
    console_write_line("AI endpoint cleared / Endpoint IA limpiado");
}

static void ai_gateway_ask(const char *text) {
    const char *prompt = skip_spaces(text);

    if (!prompt[0]) {
        console_write_line("Usage / Uso: ai ask <text>");
        return;
    }

    audit_emit_boot_event("AI_GATEWAY_ASK_STUB", prompt);

    console_write_line("Remote assistant request / Solicitud a IA remota:");
    if (g_ai_endpoint_configured) {
        write_endpoint_line();
    } else {
        console_write_line("Endpoint: not configured; using local fallback.");
        console_write_line("Endpoint no configurado; usando respuesta local.");
    }

    console_write_line("Transport result / Resultado de transporte:");
    console_write_line("Not sent yet. Network transport is planned for the next patches.");
    console_write_line("Aun no se envia. El transporte de red queda para los siguientes parches.");
    console_write_line("");

    console_write_line("Local structured fallback / Respuesta local estructurada:");
    console_write_line("1. Resumen entendido / Understood summary:");
    console_write_line(prompt);
    console_write_line("2. Riesgos inmediatos / Immediate risks:");
    console_write_line("Do not treat remote AI as available until network and audit are ready.");
    console_write_line("No trates la IA remota como disponible hasta tener red y auditoria listas.");
    console_write_line("3. Preguntas criticas / Critical questions:");
    console_write_line("Is Ethernet present? Is audit persistent? Is the remote endpoint trusted?");
    console_write_line("Hay Ethernet? La auditoria es persistente? El endpoint remoto es confiable?");
    console_write_line("4. Acciones inmediatas sugeridas / Suggested immediate actions:");
    console_write_line("Use 'net status', configure endpoint with 'ai connect', then retry when network transport exists.");
    console_write_line("Usa 'net status', configura endpoint con 'ai connect' y reintenta cuando exista transporte de red.");
    console_write_line("5. Recursos del sistema disponibles / Available system resources:");
    console_write_line("Local console, proposal engine, hardware snapshot, audit state reporting.");
    console_write_line("6. Propuesta del sistema / System proposal:");
    console_write_line("Keep this as an auditable stub until Ethernet/DHCP and secure transport are implemented.");
    console_write_line("7. Estado de auditoria / Audit state:");
    console_write_line("AI gateway command emitted an audit event when the backend is available.");
}

bool ai_gateway_handle_command(const char *line) {
    const char *cmd;

    if (!line) {
        return false;
    }

    cmd = skip_spaces(line);

    if (str_eq(cmd, "ai") || str_eq(cmd, "ai status")) {
        audit_emit_boot_event("COMMAND_AI_STATUS", "ai status");
        ai_gateway_show_status();
        return true;
    }

    if (str_eq(cmd, "ai help")) {
        audit_emit_boot_event("COMMAND_AI_HELP", "ai help");
        ai_gateway_show_help();
        return true;
    }

    if (starts_with(cmd, "ai connect ")) {
        ai_gateway_connect(cmd + 11);
        return true;
    }

    if (str_eq(cmd, "ai disconnect")) {
        ai_gateway_disconnect();
        return true;
    }

    if (starts_with(cmd, "ai ask ")) {
        ai_gateway_ask(cmd + 7);
        return true;
    }

    return false;
}
