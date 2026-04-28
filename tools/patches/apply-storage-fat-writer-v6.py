#!/usr/bin/env python3
from pathlib import Path

ROOT = Path.cwd()


def patch_file(path: str, replacements):
    p = ROOT / path
    s = p.read_text()
    original = s
    for old, new in replacements:
        if old not in s:
            raise SystemExit(f"Patch failed for {path}: pattern not found:\n{old}")
        s = s.replace(old, new, 1)
    if s != original:
        p.write_text(s)
        print(f"patched {path}")
    else:
        print(f"unchanged {path}")


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


# Makefile: add storage.c to common kernel sources.
ensure_contains_insert_after(
    "Makefile",
    "\tkernel/command_core.c \\\n",
    "\tkernel/storage.c \\\n",
)

# kmain: initialize storage early after handoff validation and console readiness.
ensure_contains_insert_after(
    "kernel/kmain.c",
    "#include <vita/proposal.h>\n",
    "#include <vita/storage.h>\n",
)

ensure_contains_insert_after(
    "kernel/kmain.c",
    "    audit_emit_boot_event(\"HANDOFF_TO_KMAIN\", \"handoff to kmain\");\n    audit_emit_boot_event(\"CONSOLE_READY\", \"console ready\");\n",
    "\n    if (storage_init(handoff)) {\n        audit_emit_boot_event(\"STORAGE_READY\", \"storage facade initialized\");\n    } else {\n        audit_emit_boot_event(\"STORAGE_LIMITED\", \"storage facade unavailable\");\n    }\n",
)

# command_core: include storage and route storage commands.
ensure_contains_insert_after(
    "kernel/command_core.c",
    "#include <vita/proposal.h>\n",
    "#include <vita/storage.h>\n",
)

ensure_contains_insert_before(
    "kernel/command_core.c",
    "    if (str_eq(cmd, \"peers\")) {\n",
    "    if (str_eq(cmd, \"storage\") || starts_with(cmd, \"storage \")) {\n        audit_emit_boot_event(\"COMMAND_STORAGE\", \"storage command\");\n        (void)storage_handle_command(cmd);\n        return VITA_COMMAND_CONTINUE;\n    }\n\n",
)

# console: show storage in menu and help. These insertions are intentionally minimal.
ensure_contains_insert_after(
    "kernel/console.c",
    "    console_write_line(\"- clear\");\n",
    "    console_write_line(\"- storage\");\n",
)

ensure_contains_insert_after(
    "kernel/console.c",
    "    console_write_line(\"clear       -> clear screen and redraw guided header\");\n",
    "    console_write_line(\"storage     -> show storage status and file writer commands\");\n",
)

print("storage FAT writer patch applied")
