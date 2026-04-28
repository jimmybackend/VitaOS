#!/usr/bin/env python3
from pathlib import Path
import sys

ROOT = Path.cwd()


def fail(msg: str) -> None:
    print(f"[session-export-v11] ERROR: {msg}", file=sys.stderr)
    sys.exit(1)


def read(path: str) -> str:
    p = ROOT / path
    if not p.exists():
        fail(f"missing required file: {path}")
    return p.read_text()


def write(path: str, text: str) -> None:
    (ROOT / path).write_text(text)


def ensure_contains(path: str, needle: str, message: str) -> None:
    text = read(path)
    if needle not in text:
        fail(message)


def patch_makefile() -> None:
    path = "Makefile"
    text = read(path)

    if "kernel/storage.c" not in text:
        fail("kernel/storage.c is not in Makefile. Apply storage writer patch before v11.")

    if "kernel/session_export.c" not in text:
        text = text.replace("\tkernel/storage.c \\", "\tkernel/storage.c \\\n\tkernel/session_export.c \\")

    write(path, text)


def patch_console() -> None:
    path = "kernel/console.c"
    text = read(path)

    if "- export session" not in text:
        if 'console_write_line("- clear");' in text:
            text = text.replace(
                'console_write_line("- clear");',
                'console_write_line("- clear");\n    console_write_line("- export session");'
            )
        elif 'console_write_line("- shutdown");' in text:
            text = text.replace(
                'console_write_line("- shutdown");',
                'console_write_line("- export session");\n    console_write_line("- shutdown");'
            )
        else:
            fail("could not find menu insertion point in kernel/console.c")

    help_line = 'console_write_line("export session -> write /vita/export/reports/last-session.txt");'
    if help_line not in text:
        if 'console_write_line("clear       -> clear screen and redraw guided header");' in text:
            text = text.replace(
                'console_write_line("clear       -> clear screen and redraw guided header");',
                'console_write_line("clear       -> clear screen and redraw guided header");\n    ' + help_line
            )
        elif 'console_write_line("shutdown    -> stop current command session");' in text:
            text = text.replace(
                'console_write_line("shutdown    -> stop current command session");',
                help_line + '\n    console_write_line("shutdown    -> stop current command session");'
            )
        else:
            fail("could not find help insertion point in kernel/console.c")

    write(path, text)


def patch_command_core() -> None:
    path = "kernel/command_core.c"
    text = read(path)

    if "#include <vita/session_export.h>" not in text:
        if "#include <vita/storage.h>" in text:
            text = text.replace(
                "#include <vita/storage.h>",
                "#include <vita/storage.h>\n#include <vita/session_export.h>"
            )
        elif "#include <vita/proposal.h>" in text:
            text = text.replace(
                "#include <vita/proposal.h>",
                "#include <vita/proposal.h>\n#include <vita/session_export.h>"
            )
        else:
            fail("could not find include insertion point in kernel/command_core.c")

    if "COMMAND_EXPORT_SESSION" not in text:
        block = '''
    if (str_eq(cmd, "export") ||
        str_eq(cmd, "export session") ||
        str_eq(cmd, "export report") ||
        str_eq(cmd, "session export")) {
        audit_emit_boot_event("COMMAND_EXPORT_SESSION", "export session");
        if (!session_export_write_report(ctx)) {
            console_write_line("export session failed / exportacion de sesion fallo");
        }
        return VITA_COMMAND_CONTINUE;
    }

'''
        marker = '    if (str_eq(cmd, "shutdown") || str_eq(cmd, "exit")) {'
        if marker not in text:
            fail("could not find shutdown command insertion point in kernel/command_core.c")
        text = text.replace(marker, block + marker)

    write(path, text)


def main() -> None:
    ensure_contains("include/vita/storage.h", "storage_write_text", "storage_write_text() missing; apply storage patch before v11")
    ensure_contains("include/vita/command.h", "vita_command_context_t", "vita_command_context_t missing")
    patch_makefile()
    patch_console()
    patch_command_core()
    print("[session-export-v11] patch applied")


if __name__ == "__main__":
    main()
