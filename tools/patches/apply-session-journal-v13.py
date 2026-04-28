#!/usr/bin/env python3
from pathlib import Path

ROOT = Path.cwd()

def read(p): return (ROOT / p).read_text()
def write(p, s): (ROOT / p).write_text(s)

def add_after(s, anchor, insert, name):
    if insert.strip() in s:
        return s
    if anchor not in s:
        raise SystemExit(f"anchor not found in {name}: {anchor!r}")
    return s.replace(anchor, anchor + insert, 1)

def add_before(s, anchor, insert, name):
    if insert.strip() in s:
        return s
    if anchor not in s:
        raise SystemExit(f"anchor not found in {name}: {anchor!r}")
    return s.replace(anchor, insert + anchor, 1)

# Makefile
s = read("Makefile")
if "kernel/session_journal.c" not in s:
    for a in ["\tkernel/session_jsonl_export.c \\\n", "\tkernel/session_export.c \\\n", "\tkernel/storage.c \\\n", "\tkernel/command_core.c \\\n"]:
        if a in s:
            s = s.replace(a, a + "\tkernel/session_journal.c \\\n", 1)
            break
    else:
        raise SystemExit("Makefile source anchor not found")
write("Makefile", s)

# kmain includes/init
s = read("kernel/kmain.c")
if "#include <vita/session_journal.h>" not in s:
    if "#include <vita/storage.h>\n" in s:
        s = s.replace("#include <vita/storage.h>\n", "#include <vita/storage.h>\n#include <vita/session_journal.h>\n", 1)
    elif "#include <vita/proposal.h>\n" in s:
        s = s.replace("#include <vita/proposal.h>\n", "#include <vita/proposal.h>\n#include <vita/session_journal.h>\n", 1)
    else:
        raise SystemExit("kmain include anchor not found")
if "SESSION_JOURNAL_ACTIVE" not in s:
    init = '''
    if (session_journal_init()) {
        audit_emit_boot_event("SESSION_JOURNAL_ACTIVE", "persistent session journal active");
    } else {
        audit_emit_boot_event("SESSION_JOURNAL_LIMITED", "persistent session journal unavailable");
    }
'''
    block = '''    if (storage_init(handoff)) {
        audit_emit_boot_event("STORAGE_READY", "storage facade initialized");
    } else {
        audit_emit_boot_event("STORAGE_LIMITED", "storage facade unavailable");
    }
'''
    if block in s:
        s = s.replace(block, block + init, 1)
    else:
        marker = '    audit_emit_boot_event("CONSOLE_READY", "console ready");\n'
        s = add_after(s, marker, init, "kernel/kmain.c")
write("kernel/kmain.c", s)

# command_core include
s = read("kernel/command_core.c")
if "#include <vita/session_journal.h>" not in s:
    for a in ["#include <vita/session_jsonl_export.h>\n", "#include <vita/session_export.h>\n", "#include <vita/storage.h>\n", "#include <vita/proposal.h>\n"]:
        if a in s:
            s = s.replace(a, a + "#include <vita/session_journal.h>\n", 1)
            break
    else:
        raise SystemExit("command_core include anchor not found")

if "session_journal_log_command(cmd);" not in s:
    s = add_after(s, "    command_refresh_state(ctx);\n\n", "    session_journal_log_command(cmd);\n\n", "kernel/command_core.c")

if "COMMAND_JOURNAL" not in s:
    handler = '''
    if (str_eq(cmd, "journal") || starts_with(cmd, "journal ")) {
        audit_emit_boot_event("COMMAND_JOURNAL", "journal command");
        (void)session_journal_handle_command(cmd);
        return VITA_COMMAND_CONTINUE;
    }

'''
    for a in ["    if (str_eq(cmd, \"storage\") || starts_with(cmd, \"storage \")) {\n", "    if (str_eq(cmd, \"export jsonl\") ||\n", "    if (str_eq(cmd, \"export\") ||\n", "    if (str_eq(cmd, \"peers\")) {\n"]:
        if a in s:
            s = s.replace(a, handler + a, 1)
            break
    else:
        raise SystemExit("command_core handler anchor not found")

if "session_journal_log_system(\"command_completed\", line);" not in s:
    old = '''        result = command_handle_line(ctx, line);
        console_pager_end();

        if (result == VITA_COMMAND_SHUTDOWN) {
            break;
        }
'''
    new = '''        result = command_handle_line(ctx, line);
        console_pager_end();

        if (result == VITA_COMMAND_SHUTDOWN) {
            session_journal_log_system("command_session_shutdown_requested", line);
            break;
        }

        session_journal_log_system("command_completed", line);
'''
    if old not in s:
        raise SystemExit("command_core command loop block not found")
    s = s.replace(old, new, 1)
write("kernel/command_core.c", s)

# console menu/help
s = read("kernel/console.c")
if 'console_write_line("- journal");' not in s:
    if 'console_write_line("- storage");\n' in s:
        s = s.replace('console_write_line("- storage");\n', 'console_write_line("- storage");\n    console_write_line("- journal");\n', 1)
    elif 'console_write_line("- shutdown");\n' in s:
        s = s.replace('console_write_line("- shutdown");\n', 'console_write_line("- journal");\n    console_write_line("- shutdown");\n', 1)
    else:
        raise SystemExit("console menu anchor not found")
if "journal     -> show persistent command/session journal status" not in s:
    if 'console_write_line("storage     -> show storage status and file writer commands");\n' in s:
        s = s.replace('console_write_line("storage     -> show storage status and file writer commands");\n', 'console_write_line("storage     -> show storage status and file writer commands");\n    console_write_line("journal     -> show persistent command/session journal status");\n', 1)
    elif 'console_write_line("shutdown    -> stop current command session");\n' in s:
        s = s.replace('console_write_line("shutdown    -> stop current command session");\n', 'console_write_line("journal     -> show persistent command/session journal status");\n    console_write_line("shutdown    -> stop current command session");\n', 1)
    else:
        raise SystemExit("console help anchor not found")
write("kernel/console.c", s)

print("session-journal-v13 applied")
