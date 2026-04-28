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

ensure_contains_insert_before(
    "kernel/command_core.c",
    "    if (str_eq(cmd, \"notes\") || str_eq(cmd, \"note\") || starts_with(cmd, \"note \") || starts_with(cmd, \"edit \")) {\n",
    "    if (str_eq(cmd, \"notes list\") || starts_with(cmd, \"cat \")) {\n        audit_emit_boot_event(\"COMMAND_STORAGE_READ\", \"storage read/list command\");\n        (void)storage_handle_command(cmd);\n        return VITA_COMMAND_CONTINUE;\n    }\n\n",
)

ensure_contains_insert_after(
    "kernel/console.c",
    "    console_write_line(\"- notes\");\n",
    "    console_write_line(\"- notes list\");\n    console_write_line(\"- cat /vita/notes/file.txt\");\n",
)

ensure_contains_insert_after(
    "kernel/console.c",
    "    console_write_line(\"notes       -> show note editor help\");\n",
    "    console_write_line(\"notes list  -> list saved note files\");\n    console_write_line(\"cat PATH    -> show a small /vita/... text file\");\n",
)

print("storage read and notes list patch applied")
