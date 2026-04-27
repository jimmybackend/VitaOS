#!/usr/bin/env python3
"""
VitaOS patch v2: detailed network readiness status.

This patch builds on the previous net/net status command and adds a small
net_status module that reports UEFI protocol readiness without connecting yet.

Run from the repository root:
    python3 tools/patches/apply-net-status-details-v2.py
"""
from pathlib import Path
import sys

ROOT = Path.cwd()
MAKEFILE = ROOT / "Makefile"
COMMAND = ROOT / "kernel" / "command_core.c"
CONSOLE = ROOT / "kernel" / "console.c"
NET_H = ROOT / "include" / "vita" / "net_status.h"
NET_C = ROOT / "kernel" / "net_status.c"


def fail(msg: str) -> None:
    print(f"[vitaos-net-details] ERROR: {msg}", file=sys.stderr)
    sys.exit(1)


def backup(path: Path) -> None:
    bak = path.with_suffix(path.suffix + ".bak-net-details-v2")
    if path.exists() and not bak.exists():
        bak.write_text(path.read_text())


def read_packaged(rel: str) -> str:
    packaged = ROOT / rel
    if not packaged.exists():
        fail(f"missing packaged file after unzip: {rel}")
    return packaged.read_text()


def ensure_new_files() -> None:
    if not NET_H.exists():
        fail("missing include/vita/net_status.h. Unzip the patch before running this script.")
    if not NET_C.exists():
        fail("missing kernel/net_status.c. Unzip the patch before running this script.")
    print("[vitaos-net-details] net_status files present")


def patch_makefile() -> None:
    if not MAKEFILE.exists():
        fail("missing Makefile")

    s = MAKEFILE.read_text()
    backup(MAKEFILE)

    if "kernel/net_status.c" in s:
        print("[vitaos-net-details] Makefile already contains kernel/net_status.c")
        return

    marker = "\tkernel/hardware_discovery.c \\\n"
    if marker not in s:
        marker = "\tkernel/command_core.c \\\n"
    if marker not in s:
        fail("could not find COMMON_KERNEL marker in Makefile")

    s = s.replace(marker, marker + "\tkernel/net_status.c \\\n", 1)
    MAKEFILE.write_text(s)
    print("[vitaos-net-details] patched Makefile")


def replace_show_net(s: str) -> str:
    new_show_net = '''
static void show_net(const vita_command_context_t *ctx) {
    if (!ctx) {
        net_status_show(0, 0, false);
        return;
    }

    net_status_show(ctx->handoff, ctx->hw_ready ? &ctx->hw_snapshot : 0, ctx->hw_ready);
}

'''

    start = s.find("static void show_net(")
    if start >= 0:
        end = s.find("static void show_audit", start)
        if end < 0:
            fail("found show_net but could not find show_audit after it")
        return s[:start] + new_show_net + s[end:]

    marker = "static void show_audit(const vita_command_context_t *ctx) {"
    if marker not in s:
        fail("could not find show_audit marker in kernel/command_core.c")
    return s.replace(marker, new_show_net + marker, 1)


def ensure_net_handler(s: str) -> str:
    if 'COMMAND_NET_STATUS' in s:
        return s

    handler = '''    if (str_eq(cmd, "net") || str_eq(cmd, "net status")) {
        audit_emit_boot_event("COMMAND_NET_STATUS", "net status");
        show_net(ctx);
        return VITA_COMMAND_CONTINUE;
    }

'''

    marker = '''    if (str_eq(cmd, "audit")) {
        audit_emit_boot_event("COMMAND_AUDIT", "audit");
        show_audit(ctx);
        return VITA_COMMAND_CONTINUE;
    }

'''
    if marker not in s:
        fail("could not find audit command marker in kernel/command_core.c")
    return s.replace(marker, handler + marker, 1)


def patch_command_core() -> None:
    if not COMMAND.exists():
        fail("missing kernel/command_core.c")

    s = COMMAND.read_text()
    backup(COMMAND)

    include_line = "#include <vita/net_status.h>\n"
    if include_line not in s:
        marker = "#include <vita/node.h>\n"
        if marker not in s:
            fail("could not find include marker in kernel/command_core.c")
        s = s.replace(marker, include_line + marker, 1)

    s = replace_show_net(s)
    s = ensure_net_handler(s)

    COMMAND.write_text(s)
    print("[vitaos-net-details] patched kernel/command_core.c")


def patch_console() -> None:
    if not CONSOLE.exists():
        fail("missing kernel/console.c")

    s = CONSOLE.read_text()
    backup(CONSOLE)

    if 'console_write_line("- net");' not in s:
        marker = '    console_write_line("- hw");\n'
        if marker not in s:
            fail("could not find hw menu marker in kernel/console.c")
        s = s.replace(marker, marker + '    console_write_line("- net");\n', 1)

    if 'net         -> show detailed network readiness status' not in s:
        old = '    console_write_line("net         -> show network readiness summary");\n'
        new = '    console_write_line("net         -> show detailed network readiness status");\n'
        if old in s:
            s = s.replace(old, new, 1)
        else:
            marker = '    console_write_line("hw          -> show hardware summary when available");\n'
            if marker not in s:
                fail("could not find hw help marker in kernel/console.c")
            s = s.replace(marker, marker + new, 1)

    CONSOLE.write_text(s)
    print("[vitaos-net-details] patched kernel/console.c")


def main() -> None:
    if not (ROOT / "Makefile").exists() or not (ROOT / "kernel").exists():
        fail("run this script from the VitaOS repository root")

    ensure_new_files()
    patch_makefile()
    patch_command_core()
    patch_console()
    print("[vitaos-net-details] done")
    print("[vitaos-net-details] recommended test:")
    print("  make clean && make hosted && make smoke-audit && make && make iso")
    print("[vitaos-net-details] command to test inside VitaOS: net")


if __name__ == "__main__":
    main()
