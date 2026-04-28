#!/usr/bin/env python3
from pathlib import Path

ROOT = Path.cwd()


def ensure_contains_insert_after(path: str, marker: str, insert: str):
    p = ROOT / path
    s = p.read_text()
    if insert.strip() in s:
        print(f"already patched {path}")
        return
    if marker not in s:
        raise SystemExit(f"Patch failed for {path}: marker not found:\n{marker}")
    s = s.replace(marker, marker + insert, 1)
    p.write_text(s)
    print(f"patched {path}")


def ensure_contains_insert_before(path: str, marker: str, insert: str):
    p = ROOT / path
    s = p.read_text()
    if insert.strip() in s:
        print(f"already patched {path}")
        return
    if marker not in s:
        raise SystemExit(f"Patch failed for {path}: marker not found:\n{marker}")
    s = s.replace(marker, insert + marker, 1)
    p.write_text(s)
    print(f"patched {path}")


# Makefile: add editor.c to common kernel sources after storage.c when available.
makefile = ROOT / "Makefile"
ms = makefile.read_text()
if "\tkernel/editor.c \\\n" not in ms:
    if "\tkernel/storage.c \\\n" in ms:
        ms = ms.replace("\tkernel/storage.c \\\n", "\tkernel/storage.c \\\n\tkernel/editor.c \\\n", 1)
    elif "\tkernel/command_core.c \\\n" in ms:
        ms = ms.replace("\tkernel/command_core.c \\\n", "\tkernel/command_core.c \\\n\tkernel/editor.c \\\n", 1)
    else:
        raise SystemExit("Patch failed for Makefile: no suitable COMMON_KERNEL anchor found")
    makefile.write_text(ms)
    print("patched Makefile")
else:
    print("already patched Makefile")

# command_core: include editor and route note/edit commands before generic emergency fallback.
ensure_contains_insert_after(
    "kernel/command_core.c",
    "#include <vita/command.h>\n",
    "#include <vita/editor.h>\n",
)

ensure_contains_insert_before(
    "kernel/command_core.c",
    "    if (starts_with(cmd, \"approve \") || starts_with(cmd, \"reject \")) {\n",
    "    if (str_eq(cmd, \"notes\") || str_eq(cmd, \"note\") || starts_with(cmd, \"note \") || starts_with(cmd, \"edit \")) {\n        audit_emit_boot_event(\"COMMAND_EDITOR\", \"note editor command\");\n        if (!editor_handle_command(cmd)) {\n            console_write_line(\"editor command failed / comando de editor no ejecutado\");\n        }\n        return VITA_COMMAND_CONTINUE;\n    }\n\n",
)

# console: add menu/help entries.
ensure_contains_insert_after(
    "kernel/console.c",
    "    console_write_line(\"- storage\");\n",
    "    console_write_line(\"- notes\");\n    console_write_line(\"- note [file]\");\n    console_write_line(\"- edit /vita/notes/file.txt\");\n",
)

ensure_contains_insert_after(
    "kernel/console.c",
    "    console_write_line(\"storage     -> show storage status and file writer commands\");\n",
    "    console_write_line(\"notes       -> show note editor help\");\n    console_write_line(\"note FILE   -> create/edit a note under /vita/notes\");\n    console_write_line(\"edit PATH   -> line editor for a /vita/... file\");\n",
)

print("note editor patch applied")
