#!/usr/bin/env python3
from pathlib import Path

ROOT = Path.cwd()


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def write(path: str, text: str) -> None:
    p = ROOT / path
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text(text, encoding="utf-8")


def ensure_contains(path: str, needle: str, insert_after: str, insertion: str) -> None:
    s = read(path)
    if needle in s:
        return
    if insert_after not in s:
        raise SystemExit(f"No encontre el bloque esperado en {path}: {insert_after!r}")
    s = s.replace(insert_after, insert_after + insertion, 1)
    write(path, s)


def patch_makefile() -> None:
    path = "Makefile"
    s = read(path)
    if "kernel/ai_gateway.c" in s:
        return

    target = "\tkernel/command_core.c \\\n"
    if target not in s:
        raise SystemExit("No encontre kernel/command_core.c en COMMON_KERNEL del Makefile")

    s = s.replace(target, target + "\tkernel/ai_gateway.c \\\n", 1)
    write(path, s)


def patch_console_menu() -> None:
    path = "kernel/console.c"
    s = read(path)

    if "- ai status" not in s:
        anchor = '    console_write_line("- helpme");\n'
        if anchor not in s:
            raise SystemExit("No encontre el menu helpme en kernel/console.c")
        s = s.replace(
            anchor,
            anchor
            + '    console_write_line("- ai status");\n'
            + '    console_write_line("- ai connect <host> <port>");\n'
            + '    console_write_line("- ai ask <text>");\n',
            1,
        )

    if "ai status   -> show remote assistant gateway status" not in s:
        anchor = '    console_write_line("clear       -> clear screen and redraw guided header");\n'
        if anchor not in s:
            # Older tree without clear line: place before approve.
            anchor = '    console_write_line("approve ID  -> approve a proposal");\n'
            insert_before = True
        else:
            insert_before = False

        addition = (
            '    console_write_line("ai status   -> show remote assistant gateway status");\n'
            '    console_write_line("ai connect  -> store remote assistant endpoint");\n'
            '    console_write_line("ai ask      -> local audited fallback until network exists");\n'
        )

        if insert_before:
            s = s.replace(anchor, addition + anchor, 1)
        else:
            s = s.replace(anchor, anchor + addition, 1)

    write(path, s)


def patch_command_core() -> None:
    path = "kernel/command_core.c"
    s = read(path)

    if "#include <vita/ai_gateway.h>" not in s:
        anchor = "#include <vita/audit.h>\n"
        if anchor not in s:
            raise SystemExit("No encontre include audit en kernel/command_core.c")
        s = s.replace(anchor, anchor + "#include <vita/ai_gateway.h>\n", 1)

    if "ai_gateway_handle_command" not in s:
        anchor = '''    if (str_eq(cmd, "helpme") || str_eq(cmd, "help") || str_eq(cmd, "menu")) {
        audit_emit_boot_event("COMMAND_HELPME", "helpme");
        console_guided_show_help();
        console_guided_show_menu();
        return VITA_COMMAND_CONTINUE;
    }

'''
        block = '''    if (str_eq(cmd, "ai") || starts_with(cmd, "ai ")) {
        if (!ai_gateway_handle_command(cmd)) {
            console_write_line("AI gateway command not recognized / Comando de puerta IA no reconocido");
            ai_gateway_show_help();
        }
        return VITA_COMMAND_CONTINUE;
    }

'''
        if anchor not in s:
            raise SystemExit("No encontre bloque help/menu en kernel/command_core.c")
        s = s.replace(anchor, anchor + block, 1)

    write(path, s)


def main() -> None:
    patch_makefile()
    patch_console_menu()
    patch_command_core()
    print("ai gateway stub patch applied")


if __name__ == "__main__":
    main()
