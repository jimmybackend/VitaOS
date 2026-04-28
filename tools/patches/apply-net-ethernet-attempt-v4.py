#!/usr/bin/env python3
from pathlib import Path
import re

ROOT = Path.cwd()

def read(path):
    return (ROOT / path).read_text()

def write(path, data):
    (ROOT / path).write_text(data)

def patch_makefile():
    p = "Makefile"
    s = read(p)
    if "kernel/net_connect.c" in s:
        return

    anchors = [
        "\tkernel/net_status.c \\\n",
        "\tkernel/node_core.c\n",
        "\tkernel/proposal.c \\\n",
    ]

    for a in anchors:
        if a in s:
            if a.endswith("node_core.c\n"):
                s = s.replace(a, "\tkernel/net_connect.c \\\n" + a, 1)
            else:
                s = s.replace(a, a + "\tkernel/net_connect.c \\\n", 1)
            write(p, s)
            return

    raise SystemExit("Could not patch Makefile: COMMON_KERNEL anchor not found")

def patch_console():
    p = "kernel/console.c"
    s = read(p)

    if "net connect <host> <port>" not in s:
        if 'console_write_line("- net status");' in s:
            s = s.replace('console_write_line("- net status");',
                          'console_write_line("- net status");\n    console_write_line("- net connect <host> <port>");\n    console_write_line("- net send <text>");\n    console_write_line("- net disconnect");', 1)
        elif 'console_write_line("- net");' in s:
            s = s.replace('console_write_line("- net");',
                          'console_write_line("- net");\n    console_write_line("- net connect <host> <port>");\n    console_write_line("- net send <text>");\n    console_write_line("- net disconnect");', 1)
        elif 'console_write_line("- peers");' in s:
            s = s.replace('console_write_line("- peers");',
                          'console_write_line("- net");\n    console_write_line("- net connect <host> <port>");\n    console_write_line("- net send <text>");\n    console_write_line("- net disconnect");\n    console_write_line("- peers");', 1)

    if "net connect HOST PORT" not in s:
        if 'console_write_line("clear       -> clear screen and redraw guided header");' in s:
            s = s.replace('console_write_line("clear       -> clear screen and redraw guided header");',
                          'console_write_line("clear       -> clear screen and redraw guided header");\n    console_write_line("net status  -> show network readiness and connection state");\n    console_write_line("net connect HOST PORT -> attempt hosted TCP connection / intento TCP hosted");\n    console_write_line("net send TEXT -> send a line over active hosted TCP connection");\n    console_write_line("net disconnect -> close active network session");', 1)
        elif 'console_write_line("net status' in s:
            s = s.replace('console_write_line("net status',
                          'console_write_line("net connect HOST PORT -> attempt hosted TCP connection / intento TCP hosted");\n    console_write_line("net send TEXT -> send a line over active hosted TCP connection");\n    console_write_line("net disconnect -> close active network session");\n    console_write_line("net status', 1)

    write(p, s)

def patch_command_core():
    p = "kernel/command_core.c"
    s = read(p)

    if "#include <vita/net_connect.h>" not in s:
        if "#include <vita/net_status.h>" in s:
            s = s.replace("#include <vita/net_status.h>\n", "#include <vita/net_status.h>\n#include <vita/net_connect.h>\n", 1)
        elif "#include <vita/node.h>\n" in s:
            s = s.replace("#include <vita/node.h>\n", "#include <vita/node.h>\n#include <vita/net_connect.h>\n", 1)
        else:
            raise SystemExit("Could not patch command_core.c: include anchor not found")

    # Remove older minimal net status block if present.
    s = re.sub(
        r'\n    if \(str_eq\(cmd, "net"\) \|\| str_eq\(cmd, "net status"\)\) \{.*?\n    \}\n\n',
        '\n',
        s,
        flags=re.S
    )

    block = '''
    if (str_eq(cmd, "net") || str_eq(cmd, "net status")) {
        audit_emit_boot_event("COMMAND_NET_STATUS", "net status");
        net_connect_show_status(ctx->handoff, ctx->hw_ready ? &ctx->hw_snapshot : 0, ctx->hw_ready);
        return VITA_COMMAND_CONTINUE;
    }

    if (starts_with(cmd, "net connect ")) {
        const char *endpoint = cmd + 12;

        audit_emit_boot_event("COMMAND_NET_CONNECT", endpoint);
        if (!net_connect_configure_endpoint(endpoint)) {
            console_write_line("net connect failed / fallo net connect:");
            console_write_line(net_connect_last_error());
            return VITA_COMMAND_CONTINUE;
        }

        if (net_connect_attempt()) {
            console_write_line("network connected / red conectada");
        } else {
            console_write_line("network connection not active / conexion de red no activa:");
            console_write_line(net_connect_last_error());
        }

        return VITA_COMMAND_CONTINUE;
    }

    if (starts_with(cmd, "net send ")) {
        const char *payload = cmd + 9;

        audit_emit_boot_event("COMMAND_NET_SEND", "net send");
        if (net_connect_send_line(payload)) {
            console_write_line("network line sent / linea enviada por red");
        } else {
            console_write_line("network send failed / envio de red fallo:");
            console_write_line(net_connect_last_error());
        }

        return VITA_COMMAND_CONTINUE;
    }

    if (str_eq(cmd, "net disconnect")) {
        audit_emit_boot_event("COMMAND_NET_DISCONNECT", "net disconnect");
        net_connect_disconnect();
        console_write_line("network disconnected / red desconectada");
        return VITA_COMMAND_CONTINUE;
    }

'''

    if "COMMAND_NET_CONNECT" not in s:
        if '    if (str_eq(cmd, "peers")) {' in s:
            s = s.replace('    if (str_eq(cmd, "peers")) {', block + '    if (str_eq(cmd, "peers")) {', 1)
        elif '    if (str_eq(cmd, "proposals") || str_eq(cmd, "list")) {' in s:
            s = s.replace('    if (str_eq(cmd, "proposals") || str_eq(cmd, "list")) {', block + '    if (str_eq(cmd, "proposals") || str_eq(cmd, "list")) {', 1)
        else:
            raise SystemExit("Could not patch command_core.c: command anchor not found")

    write(p, s)

def main():
    patch_makefile()
    patch_console()
    patch_command_core()
    print("net ethernet attempt v4 patch applied")

if __name__ == "__main__":
    main()
