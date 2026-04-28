/*
 * kernel/net_connect.c
 * Minimal F1A/F1B network connection attempt layer.
 *
 * Scope:
 * - Hosted: TCP connect/send to an IPv4 literal endpoint for validation.
 * - UEFI: diagnostic-only until UEFI TCP4/DHCP4 transport is implemented.
 *
 * This module does not implement DHCP, DNS, TLS, Wi-Fi association,
 * sockets for UEFI, or a full network stack.
 */

#include <vita/console.h>
#include <vita/net_connect.h>

#ifdef VITA_HOSTED
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#define NET_CONNECT_HOST_MAX 64

static char g_host[NET_CONNECT_HOST_MAX];
static uint16_t g_port = 0;
static vita_net_connect_state_t g_state = VITA_NET_CONNECT_DISCONNECTED;
static char g_last_error[96] = "not connected";

#ifdef VITA_HOSTED
static int g_fd = -1;
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

static void set_error(const char *text) {
    copy_text(g_last_error, sizeof(g_last_error), text ? text : "unknown error");
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

static bool parse_u16(const char *text, uint16_t *out_port) {
    unsigned long value = 0;
    unsigned long digits = 0;

    if (!text || !out_port) {
        return false;
    }

    text = skip_spaces(text);
    while (*text >= '0' && *text <= '9') {
        value = (value * 10UL) + (unsigned long)(*text - '0');
        if (value > 65535UL) {
            return false;
        }
        digits++;
        text++;
    }

    while (*text) {
        if (!is_space_char(*text)) {
            return false;
        }
        text++;
    }

    if (digits == 0 || value == 0) {
        return false;
    }

    *out_port = (uint16_t)value;
    return true;
}

void net_connect_reset(void) {
#ifdef VITA_HOSTED
    if (g_fd >= 0) {
        close(g_fd);
        g_fd = -1;
    }
#endif
    g_host[0] = '\0';
    g_port = 0;
    g_state = VITA_NET_CONNECT_DISCONNECTED;
    set_error("not connected");
}

bool net_connect_configure_endpoint(const char *endpoint_text) {
    const char *p;
    unsigned long n = 0;
    uint16_t port = 0;

    if (!endpoint_text) {
        set_error("missing endpoint");
        return false;
    }

    p = skip_spaces(endpoint_text);

    while (p[n] && !is_space_char(p[n]) && p[n] != ':' && (n + 1) < sizeof(g_host)) {
        g_host[n] = p[n];
        n++;
    }
    g_host[n] = '\0';

    if (n == 0) {
        set_error("missing host");
        return false;
    }

    p += n;
    if (*p == ':') {
        p++;
    }

    if (!parse_u16(p, &port)) {
        g_host[0] = '\0';
        set_error("expected endpoint format: <ipv4> <port> or <ipv4>:<port>");
        return false;
    }

    g_port = port;
    g_state = VITA_NET_CONNECT_CONFIGURED;
    set_error("configured, not connected");
    return true;
}

bool net_connect_attempt(void) {
#ifdef VITA_HOSTED
    struct sockaddr_in addr;
    int fd;

    if (!g_host[0] || g_port == 0) {
        set_error("endpoint not configured");
        return false;
    }

    if (g_fd >= 0) {
        close(g_fd);
        g_fd = -1;
    }

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        g_state = VITA_NET_CONNECT_CONFIGURED;
        set_error("socket failed");
        return false;
    }

    addr.sin_family = AF_INET;
    addr.sin_port = htons(g_port);
    addr.sin_addr.s_addr = 0;

    if (inet_pton(AF_INET, g_host, &addr.sin_addr) != 1) {
        close(fd);
        g_state = VITA_NET_CONNECT_CONFIGURED;
        set_error("host must be an IPv4 literal in this milestone");
        return false;
    }

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(fd);
        g_state = VITA_NET_CONNECT_CONFIGURED;
        set_error("tcp connect failed");
        return false;
    }

    g_fd = fd;
    g_state = VITA_NET_CONNECT_CONNECTED;
    set_error("connected");
    return true;
#else
    if (!g_host[0] || g_port == 0) {
        set_error("endpoint not configured");
        return false;
    }

    g_state = VITA_NET_CONNECT_LIMITED;
    set_error("UEFI TCP transport not implemented yet; status-only mode");
    return false;
#endif
}

void net_connect_disconnect(void) {
#ifdef VITA_HOSTED
    if (g_fd >= 0) {
        close(g_fd);
        g_fd = -1;
    }
#endif

    if (g_host[0] && g_port != 0) {
        g_state = VITA_NET_CONNECT_CONFIGURED;
        set_error("disconnected; endpoint still configured");
    } else {
        g_state = VITA_NET_CONNECT_DISCONNECTED;
        set_error("not connected");
    }
}

bool net_connect_send_line(const char *line) {
#ifdef VITA_HOSTED
    unsigned long len = 0;

    if (g_state != VITA_NET_CONNECT_CONNECTED || g_fd < 0) {
        set_error("not connected");
        return false;
    }

    if (!line) {
        set_error("empty send line");
        return false;
    }

    while (line[len]) {
        len++;
    }

    if (len > 0) {
        if (send(g_fd, line, len, 0) < 0) {
            net_connect_disconnect();
            set_error("send failed");
            return false;
        }
    }

    if (send(g_fd, "\n", 1, 0) < 0) {
        net_connect_disconnect();
        set_error("send newline failed");
        return false;
    }

    set_error("line sent");
    return true;
#else
    (void)line;
    set_error("UEFI TCP send not implemented yet");
    return false;
#endif
}

bool net_connect_is_configured(void) {
    return g_host[0] != '\0' && g_port != 0;
}

bool net_connect_is_connected(void) {
    return g_state == VITA_NET_CONNECT_CONNECTED;
}

const char *net_connect_host(void) {
    return g_host[0] ? g_host : "(none)";
}

uint16_t net_connect_port(void) {
    return g_port;
}

const char *net_connect_state_name(void) {
    switch (g_state) {
        case VITA_NET_CONNECT_DISCONNECTED:
            return "disconnected";
        case VITA_NET_CONNECT_CONFIGURED:
            return "configured";
        case VITA_NET_CONNECT_CONNECTED:
            return "connected";
        case VITA_NET_CONNECT_LIMITED:
            return "limited";
        default:
            return "unknown";
    }
}

const char *net_connect_last_error(void) {
    return g_last_error;
}

static void u16_to_dec(uint16_t v, char out[8]) {
    char tmp[8];
    int i = 0;
    int j = 0;

    if (!out) {
        return;
    }

    if (v == 0) {
        out[0] = '0';
        out[1] = '\0';
        return;
    }

    while (v > 0 && i < (int)(sizeof(tmp) - 1)) {
        tmp[i++] = (char)('0' + (v % 10U));
        v = (uint16_t)(v / 10U);
    }

    while (i > 0) {
        out[j++] = tmp[--i];
    }

    out[j] = '\0';
}

static void i32_to_dec(int v, char out[16]) {
    char tmp[16];
    unsigned int value;
    int i = 0;
    int j = 0;

    if (!out) {
        return;
    }

    if (v < 0) {
        value = 0;
    } else {
        value = (unsigned int)v;
    }

    if (value == 0U) {
        out[0] = '0';
        out[1] = '\0';
        return;
    }

    while (value > 0U && i < (int)(sizeof(tmp) - 1)) {
        tmp[i++] = (char)('0' + (value % 10U));
        value /= 10U;
    }

    while (i > 0) {
        out[j++] = tmp[--i];
    }

    out[j] = '\0';
}

static void show_count(const char *label, int value) {
    char num[16];

    console_write_line(label);
    i32_to_dec(value, num);
    console_write_line(num);
}

void net_connect_show_status(const vita_handoff_t *handoff,
                             const vita_hw_snapshot_t *hw,
                             bool hw_ready) {
    char port_text[8];

    console_write_line("Network connection / Conexion de red:");
    console_write_line("scope:");
#ifdef VITA_HOSTED
    console_write_line("hosted tcp validation path");
#else
    console_write_line("uefi diagnostic path");
#endif

    console_write_line("boot_mode:");
    if (handoff && handoff->firmware_type == VITA_FIRMWARE_HOSTED) {
        console_write_line("hosted");
    } else if (handoff && handoff->firmware_type == VITA_FIRMWARE_UEFI) {
        console_write_line("uefi");
    } else {
        console_write_line("unknown");
    }

    if (hw_ready && hw) {
        show_count("net_count:", hw->net_count);
        show_count("ethernet_count:", hw->ethernet_count);
        show_count("wifi_count:", hw->wifi_count);
    } else {
        console_write_line("network hardware snapshot unavailable");
    }

    console_write_line("state:");
    console_write_line(net_connect_state_name());

    console_write_line("endpoint_host:");
    console_write_line(net_connect_host());

    console_write_line("endpoint_port:");
    u16_to_dec(g_port, port_text);
    console_write_line(port_text);

    console_write_line("last_status:");
    console_write_line(net_connect_last_error());

#ifdef VITA_HOSTED
    console_write_line("transport:");
    console_write_line("TCP connect/send is available for IPv4 literal endpoints.");
#else
    console_write_line("transport:");
    console_write_line("UEFI network transport is not active yet.");
    console_write_line("Use this command to validate endpoint intent; real TCP/DHCP comes in the UEFI transport patch.");
#endif

    console_write_line("commands:");
    console_write_line("net connect <ipv4> <port>");
    console_write_line("net send <text>");
    console_write_line("net disconnect");
    console_write_line("net status");
}
