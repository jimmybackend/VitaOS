#!/usr/bin/env python3
from pathlib import Path

ROOT = Path.cwd()


def replace_condition(path: str, old: str, new: str):
    p = ROOT / path
    s = p.read_text()
    if new in s:
        print(f"already patched {path}")
        return
    if old not in s:
        raise SystemExit(f"Patch failed for {path}: marker not found:\n{old}")
    p.write_text(s.replace(old, new, 1))
    print(f"patched {path}")


def insert_after(path: str, marker: str, insert: str):
    p = ROOT / path
    s = p.read_text()
    if insert.strip() in s:
        print(f"already patched {path}")
        return
    if marker not in s:
        raise SystemExit(f"Patch failed for {path}: marker not found:\n{marker}")
    p.write_text(s.replace(marker, marker + insert, 1))
    print(f"patched {path}")

# Route append commands to the editor command handler.
replace_condition(
    "kernel/command_core.c",
    "    if (str_eq(cmd, \"notes\") || str_eq(cmd, \"note\") || starts_with(cmd, \"note \") || starts_with(cmd, \"edit \")) {\n",
    "    if (str_eq(cmd, \"notes\") || str_eq(cmd, \"note\") || starts_with(cmd, \"note \") || str_eq(cmd, \"append\") || starts_with(cmd, \"append \") || starts_with(cmd, \"edit \")) {\n",
)

# Add menu/help entries. The exact menu may already contain note/edit lines from earlier patches.
insert_after(
    "kernel/console.c",
    "    console_write_line(\"- note [file]\");\n",
    "    console_write_line(\"- append [file]\");\n",
)

insert_after(
    "kernel/console.c",
    "    console_write_line(\"note FILE   -> create/edit a note under /vita/notes\");\n",
    "    console_write_line(\"append FILE -> append safely to a note under /vita/notes\");\n",
)

# If older help text does not have the exact note marker, try a fallback.
console = ROOT / "kernel/console.c"
s = console.read_text()
if "append FILE -> append safely to a note under /vita/notes" not in s:
    fallback = "    console_write_line(\"edit PATH   -> line editor for a /vita/... file\");\n"
    if fallback in s:
        s = s.replace(fallback, "    console_write_line(\"append FILE -> append safely to a note under /vita/notes\");\n" + fallback, 1)
        console.write_text(s)
        print("patched kernel/console.c fallback help")

print("note editor safe edit patch applied")
