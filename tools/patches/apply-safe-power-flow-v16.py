#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def read(path):
    return (ROOT / path).read_text()


def write(path, text):
    (ROOT / path).write_text(text)


def patch_makefile():
    path = "Makefile"
    text = read(path)
    if "kernel/power.c" not in text:
        needle = "\tkernel/command_core.c \\\n"
        if needle not in text:
            raise SystemExit("Makefile: could not find command_core.c in COMMON_KERNEL")
        text = text.replace(needle, needle + "\tkernel/power.c \\\n", 1)
    write(path, text)


def patch_console():
    path = "kernel/console.c"
    text = read(path)

    if "- power" not in text:
        marker = '    console_write_line("- clear");\n'
        if marker in text:
            text = text.replace(marker, marker + '    console_write_line("- power");\n', 1)

    if "power       -> show restart/shutdown readiness" not in text:
        marker = '    console_write_line("clear       -> clear screen and redraw guided header");\n'
        if marker in text:
            text = text.replace(
                marker,
                marker + '    console_write_line("power       -> show restart/shutdown readiness");\n'
                         '    console_write_line("restart     -> flush journal and request firmware restart");\n'
                         '    console_write_line("poweroff    -> flush journal and request firmware shutdown");\n',
                1,
            )

    write(path, text)


def replace_shutdown_block(text):
    old = """    if (str_eq(cmd, \"shutdown\") || str_eq(cmd, \"exit\")) {
        audit_emit_boot_event(\"COMMAND_SHUTDOWN\", \"shutdown\");
        console_write_line(\"Shutting down command session / Cerrando sesion de comandos\");
        return VITA_COMMAND_SHUTDOWN;
    }
"""
    new = """    if (str_eq(cmd, \"restart\") || str_eq(cmd, \"reboot\")) {
        audit_emit_boot_event(\"COMMAND_RESTART\", \"restart\");
        if (power_request(VITA_POWER_ACTION_RESTART, ctx)) {
            return VITA_COMMAND_SHUTDOWN;
        }
        return VITA_COMMAND_CONTINUE;
    }

    if (str_eq(cmd, \"shutdown\") || str_eq(cmd, \"poweroff\") || str_eq(cmd, \"exit\")) {
        audit_emit_boot_event(\"COMMAND_SHUTDOWN\", \"shutdown\");
        if (power_request(VITA_POWER_ACTION_SHUTDOWN, ctx)) {
            return VITA_COMMAND_SHUTDOWN;
        }
        return VITA_COMMAND_CONTINUE;
    }
"""
    if old in text:
        return text.replace(old, new, 1)

    if "VITA_POWER_ACTION_SHUTDOWN" in text:
        return text

    marker = """    if (starts_with(cmd, \"approve \") || starts_with(cmd, \"reject \")) {"""
    insert = """    if (str_eq(cmd, \"restart\") || str_eq(cmd, \"reboot\")) {
        audit_emit_boot_event(\"COMMAND_RESTART\", \"restart\");
        if (power_request(VITA_POWER_ACTION_RESTART, ctx)) {
            return VITA_COMMAND_SHUTDOWN;
        }
        return VITA_COMMAND_CONTINUE;
    }

    if (str_eq(cmd, \"shutdown\") || str_eq(cmd, \"poweroff\") || str_eq(cmd, \"exit\")) {
        audit_emit_boot_event(\"COMMAND_SHUTDOWN\", \"shutdown\");
        if (power_request(VITA_POWER_ACTION_SHUTDOWN, ctx)) {
            return VITA_COMMAND_SHUTDOWN;
        }
        return VITA_COMMAND_CONTINUE;
    }

"""
    if marker not in text:
        raise SystemExit("kernel/command_core.c: could not find place to insert power commands")
    return text.replace(marker, insert + marker, 1)


def patch_command_core():
    path = "kernel/command_core.c"
    text = read(path)

    if "#include <vita/power.h>" not in text:
        if "#include <vita/proposal.h>" in text:
            text = text.replace("#include <vita/proposal.h>\n", "#include <vita/proposal.h>\n#include <vita/power.h>\n", 1)
        else:
            raise SystemExit("kernel/command_core.c: could not find include block")

    if "power_show_status(ctx)" not in text:
        marker = """    if (str_eq(cmd, \"status\")) {"""
        block = """    if (str_eq(cmd, \"power\") || str_eq(cmd, \"power status\")) {
        audit_emit_boot_event(\"COMMAND_POWER_STATUS\", \"power status\");
        power_show_status(ctx);
        return VITA_COMMAND_CONTINUE;
    }

"""
        if marker not in text:
            raise SystemExit("kernel/command_core.c: could not find status command block")
        text = text.replace(marker, block + marker, 1)

    text = replace_shutdown_block(text)
    write(path, text)


def main():
    for required in ["Makefile", "kernel/command_core.c", "kernel/console.c", "include/vita/session_journal.h"]:
        if not (ROOT / required).exists():
            raise SystemExit(f"missing required file: {required}")

    patch_makefile()
    patch_console()
    patch_command_core()
    print("safe power flow v16 patch applied")


if __name__ == "__main__":
    main()
