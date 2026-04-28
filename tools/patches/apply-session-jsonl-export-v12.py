#!/usr/bin/env python3
from pathlib import Path

ROOT = Path.cwd()

def read(path):
    return (ROOT / path).read_text()

def write(path, text):
    (ROOT / path).write_text(text)

def ensure_contains(path, needle, insert_after=None, insert_before=None, text=None):
    s = read(path)
    if needle in s:
        return False
    if text is None:
        text = needle
    if insert_after and insert_after in s:
        s = s.replace(insert_after, insert_after + text, 1)
    elif insert_before and insert_before in s:
        s = s.replace(insert_before, text + insert_before, 1)
    else:
        raise SystemExit(f"Could not patch {path}: anchor not found")
    write(path, s)
    return True

# Makefile: add kernel/session_jsonl_export.c to common kernel sources.
s = read("Makefile")
if "kernel/session_jsonl_export.c" not in s:
    anchors = [
        "\tkernel/session_export.c \\\n",
        "\tkernel/storage.c \\\n",
        "\tkernel/ai_gateway.c \\\n",
        "\tkernel/command_core.c \\\n",
    ]
    for anchor in anchors:
        if anchor in s:
            s = s.replace(anchor, anchor + "\tkernel/session_jsonl_export.c \\\n", 1)
            break
    else:
        raise SystemExit("Could not patch Makefile: no source anchor found")
    write("Makefile", s)

# command_core.c: include header and command handler.
s = read("kernel/command_core.c")
if "#include <vita/session_jsonl_export.h>" not in s:
    if "#include <vita/session_export.h>" in s:
        s = s.replace("#include <vita/session_export.h>\n", "#include <vita/session_export.h>\n#include <vita/session_jsonl_export.h>\n", 1)
    elif "#include <vita/proposal.h>" in s:
        s = s.replace("#include <vita/proposal.h>\n", "#include <vita/proposal.h>\n#include <vita/session_jsonl_export.h>\n", 1)
    else:
        raise SystemExit("Could not patch command_core.c include")

handler = '''
    if (str_eq(cmd, "export jsonl") ||
        str_eq(cmd, "export session jsonl") ||
        str_eq(cmd, "session export jsonl") ||
        str_eq(cmd, "jsonl export")) {
        audit_emit_boot_event("COMMAND_EXPORT_JSONL", "export jsonl");
        (void)session_export_write_jsonl(ctx);
        return VITA_COMMAND_CONTINUE;
    }

'''
if "COMMAND_EXPORT_JSONL" not in s:
    anchors = [
        "    if (str_eq(cmd, \"shutdown\") || str_eq(cmd, \"exit\")) {\n",
        "    if (starts_with(cmd, \"approve \") || starts_with(cmd, \"reject \")) {\n",
    ]
    for anchor in anchors:
        if anchor in s:
            s = s.replace(anchor, handler + anchor, 1)
            break
    else:
        raise SystemExit("Could not patch command_core.c handler")
write("kernel/command_core.c", s)

# console.c: menu/help entries.
s = read("kernel/console.c")
if "- export jsonl" not in s:
    if 'console_write_line("- export");' in s:
        s = s.replace('console_write_line("- export");\n', 'console_write_line("- export");\n    console_write_line("- export jsonl");\n', 1)
    elif 'console_write_line("- shutdown");' in s:
        s = s.replace('console_write_line("- shutdown");\n', 'console_write_line("- export jsonl");\n    console_write_line("- shutdown");\n', 1)
    else:
        raise SystemExit("Could not patch console.c menu")

if "export jsonl -> write machine-readable JSONL session report" not in s:
    if 'console_write_line("export      -> write session summary report");' in s:
        s = s.replace(
            'console_write_line("export      -> write session summary report");\n',
            'console_write_line("export      -> write session summary report");\n    console_write_line("export jsonl -> write machine-readable JSONL session report");\n',
            1,
        )
    elif 'console_write_line("shutdown    -> stop current command session");' in s:
        s = s.replace(
            'console_write_line("shutdown    -> stop current command session");\n',
            'console_write_line("export jsonl -> write machine-readable JSONL session report");\n    console_write_line("shutdown    -> stop current command session");\n',
            1,
        )
    else:
        raise SystemExit("Could not patch console.c help")
write("kernel/console.c", s)

print("session-jsonl-export-v12 applied")
