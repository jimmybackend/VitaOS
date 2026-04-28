#!/usr/bin/env bash
set -euo pipefail

# VitaOS patch v23: journal boot recovery and last-session summary.
# Bash/perl only. No Python, no malloc, no GUI/window manager.

ROOT=$(pwd)
JOURNAL_C="$ROOT/kernel/session_journal.c"
JOURNAL_H="$ROOT/include/vita/session_journal.h"
DOC_DIR="$ROOT/docs/audit"
DOC="$DOC_DIR/journal-recovery-summary.md"

if [[ ! -f "$ROOT/Makefile" || ! -f "$JOURNAL_C" || ! -f "$JOURNAL_H" ]]; then
  echo "[v23] ERROR: run this from the VitaOS repository root" >&2
  exit 1
fi

mkdir -p "$DOC_DIR"

# Public prototypes.
if ! grep -q 'session_journal_show_summary' "$JOURNAL_H"; then
  perl -0pi -e 's/(bool session_journal_flush\(void\);\n)/$1void session_journal_show_summary(void);\nbool session_journal_recover_last(void);\n/' "$JOURNAL_H"
fi

# Constants for a bounded RAM-only previous-session preview.
if ! grep -q 'SESSION_JOURNAL_RECOVERY_MAX' "$JOURNAL_C"; then
  perl -0pi -e 's/(#define SESSION_JOURNAL_EVENT_TEXT_MAX 160U\n)/$1#define SESSION_JOURNAL_RECOVERY_MAX 2048U\n#define SESSION_JOURNAL_RECOVERY_PREVIEW_LINES 18U\n/' "$JOURNAL_C"
fi

# State fields. Static, bounded, no malloc.
if ! grep -q 'recovered_available' "$JOURNAL_C"; then
  perl -0pi -e 's/(    char last_error\[VITA_STORAGE_ERROR_MAX\];\n)/$1    bool recovered_available;\n    char recovered_text[SESSION_JOURNAL_RECOVERY_MAX];\n    unsigned long recovered_len;\n/' "$JOURNAL_C"
fi

# Reset recovery metadata on init before possible capture.
if ! grep -q 'g_journal.recovered_text\[0\]' "$JOURNAL_C"; then
  perl -0pi -e 's/(    g_journal\.text\[0\] = '\''\\0'\'';\n)/$1    g_journal.recovered_available = false;\n    g_journal.recovered_len = 0;\n    g_journal.recovered_text[0] = '\''\\0'\'';\n/' "$JOURNAL_C"
fi

# Recovery/summary helpers.
if ! grep -q 'session_journal_capture_previous' "$JOURNAL_C"; then
  perl -0pi -e 's/(bool session_journal_flush\(void\) \{)/static unsigned long count_text_lines(const char *text) {\n    unsigned long i;\n    unsigned long lines = 0;\n\n    if (!text || !text[0]) {\n        return 0;\n    }\n\n    for (i = 0; text[i]; ++i) {\n        if (text[i] == '\''\\n'\'') {\n            lines++;\n        }\n    }\n\n    if (i > 0 && text[i - 1U] != '\''\\n'\'') {\n        lines++;\n    }\n\n    return lines;\n}\n\nstatic unsigned long count_token_records(const char *text, const char *token) {\n    unsigned long i;\n    unsigned long count = 0;\n\n    if (!text || !token || !token[0]) {\n        return 0;\n    }\n\n    for (i = 0; text[i]; ++i) {\n        if ((i == 0U || text[i - 1U] == '\''\\n'\'') && contains_token(text + i, token)) {\n            count++;\n        }\n    }\n\n    return count;\n}\n\nstatic void print_tail_lines(const char *text, unsigned long max_lines) {\n    unsigned long total;\n    unsigned long skip;\n    unsigned long seen = 0;\n    unsigned long i = 0;\n    unsigned long j = 0;\n    char line[160];\n\n    if (!text || !text[0]) {\n        console_write_line("(empty / vacio)");\n        return;\n    }\n\n    total = count_text_lines(text);\n    skip = (total > max_lines) ? (total - max_lines) : 0U;\n\n    while (text[i] && seen < skip) {\n        if (text[i++] == '\''\\n'\'') {\n            seen++;\n        }\n    }\n\n    while (text[i]) {\n        char c = text[i++];\n\n        if (c == '\''\\n'\'' || (j + 1U) >= sizeof(line)) {\n            line[j] = '\''\\0'\'';\n            console_write_line(line);\n            j = 0;\n            if (c != '\''\\n'\'') {\n                line[j++] = c;\n            }\n            continue;\n        }\n\n        line[j++] = c;\n    }\n\n    if (j > 0U) {\n        line[j] = '\''\\0'\'';\n        console_write_line(line);\n    }\n}\n\nstatic void session_journal_capture_previous(void) {\n    char previous[SESSION_JOURNAL_RECOVERY_MAX];\n\n    g_journal.recovered_available = false;\n    g_journal.recovered_len = 0;\n    g_journal.recovered_text[0] = '\''\\0'\'';\n\n    if (!storage_read_text(SESSION_JOURNAL_TEXT_PATH, previous, sizeof(previous))) {\n        return;\n    }\n\n    if (!previous[0]) {\n        return;\n    }\n\n    str_copy(g_journal.recovered_text, sizeof(g_journal.recovered_text), previous);\n    g_journal.recovered_len = str_len(g_journal.recovered_text);\n    g_journal.recovered_available = g_journal.recovered_len > 0U;\n}\n\nvoid session_journal_show_summary(void) {\n    char seq[16];\n    char current_lines[16];\n    char previous_lines[16];\n    char previous_commands[16];\n    char previous_notes[16];\n\n    u32_to_dec(g_journal.seq, seq);\n    u32_to_dec((uint32_t)count_text_lines(g_journal.text), current_lines);\n    u32_to_dec((uint32_t)count_text_lines(g_journal.recovered_text), previous_lines);\n    u32_to_dec((uint32_t)count_token_records(g_journal.recovered_text, "user_command"), previous_commands);\n    u32_to_dec((uint32_t)count_token_records(g_journal.recovered_text, "operator_note"), previous_notes);\n\n    console_write_line("Journal summary / Resumen de bitacora:");\n    console_write_line(g_journal.active ? "active: yes" : "active: no");\n    console_write_line(g_journal.last_flush_ok ? "last_flush: ok" : "last_flush: not-ok");\n    console_write_line(g_journal.truncated ? "current_truncated: yes" : "current_truncated: no");\n    console_write_line("current_seq:");\n    console_write_line(seq);\n    console_write_line("current_lines:");\n    console_write_line(current_lines);\n    console_write_line("previous_session_available:");\n    console_write_line(g_journal.recovered_available ? "yes" : "no");\n    console_write_line("previous_session_lines:");\n    console_write_line(previous_lines);\n    console_write_line("previous_user_command_records:");\n    console_write_line(previous_commands);\n    console_write_line("previous_operator_notes:");\n    console_write_line(previous_notes);\n    console_write_line("Commands / Comandos:");\n    console_write_line("journal last");\n    console_write_line("journal recover");\n}\n\nbool session_journal_recover_last(void) {\n    char lines[16];\n\n    if (!g_journal.recovered_available) {\n        console_write_line("journal recover: no previous session captured");\n        console_write_line("journal recover: no hay sesion anterior capturada");\n        return false;\n    }\n\n    u32_to_dec((uint32_t)count_text_lines(g_journal.recovered_text), lines);\n    console_write_line("Previous journal preview / Vista previa de bitacora anterior:");\n    console_write_line("stored_lines:");\n    console_write_line(lines);\n    console_write_line("tail_preview:");\n    print_tail_lines(g_journal.recovered_text, SESSION_JOURNAL_RECOVERY_PREVIEW_LINES);\n    return true;\n}\n\n$1/' "$JOURNAL_C"
fi

# Capture previous persisted text before this boot overwrites session-journal.txt.
if ! grep -q 'session_journal_capture_previous();' "$JOURNAL_C"; then
  perl -0pi -e 's/(    if \(!g_journal\.active\) \{\n        storage_get_status\(&st\);\n        set_error\(st\.last_error\[0\] \? st\.last_error : "storage unavailable"\);\n        console_write_line\("Persistent journal: unavailable \/ Bitacora persistente: no disponible"\);\n        console_write_line\(g_journal\.last_error\);\n        return false;\n    \}\n\n)/$1    session_journal_capture_previous();\n\n/' "$JOURNAL_C"
fi

# Show recovery status in journal status.
if ! grep -q 'previous_session_captured' "$JOURNAL_C"; then
  perl -0pi -e 's/(    console_write_line\("last_error:"\);\n    console_write_line\(g_journal\.last_error\[0\] \? g_journal\.last_error : "none"\);\n)/$1    console_write_line("previous_session_captured:");\n    console_write_line(g_journal.recovered_available ? "yes" : "no");\n/' "$JOURNAL_C"
fi

# Command aliases.
if ! grep -q 'journal summary' "$JOURNAL_C"; then
  perl -0pi -e 's/(    if \(str_eq\(cmd, "journal paths"\)\) \{\n        console_write_line\(SESSION_JOURNAL_JSONL_PATH\);\n        console_write_line\(SESSION_JOURNAL_TEXT_PATH\);\n        return true;\n    \}\n\n)/$1    if (str_eq(cmd, "journal summary") || str_eq(cmd, "journal last-session")) {\n        session_journal_show_summary();\n        return true;\n    }\n\n    if (str_eq(cmd, "journal last") || str_eq(cmd, "journal recover")) {\n        return session_journal_recover_last();\n    }\n\n/' "$JOURNAL_C"
fi

# Help text.
if ! grep -q 'journal summary' "$JOURNAL_C"; then
  echo "[v23] ERROR: failed to insert journal summary command" >&2
  exit 1
fi

if ! grep -q 'journal last-session' "$JOURNAL_C"; then
  perl -0pi -e 's/(        console_write_line\("journal paths"\);\n)/$1        console_write_line("journal summary");\n        console_write_line("journal last-session");\n        console_write_line("journal last");\n        console_write_line("journal recover");\n/' "$JOURNAL_C"
fi

cat > "$DOC" <<'DOC'
# Journal recovery and last-session summary

Patch: `journal: add boot recovery and last-session summary`

## Purpose

The session journal is a visible temporary audit bridge before full UEFI SQLite persistence. It records submitted commands after Enter and summarized system events; it is not a hidden keylogger.

This patch adds a small boot-time recovery snapshot of the previously persisted text journal before the current boot writes the new session journal files.

## Commands

- `journal summary`
- `journal last-session`
- `journal last`
- `journal recover`

## Behavior

At `session_journal_init()`, VitaOS attempts to read:

```text
/vita/audit/session-journal.txt
```

If present, a bounded RAM-only preview is kept in memory for the current boot. The current journal then starts normally and continues writing:

```text
/vita/audit/session-journal.txt
/vita/audit/session-journal.jsonl
```

## Scope

Included:

- bounded 2048-byte previous-session preview;
- no malloc;
- no hidden keystroke logging;
- redaction behavior preserved;
- text console commands only.

Not included:

- SQLite UEFI persistence;
- hash-chain verification;
- compression/encryption;
- GUI/window manager;
- Python;
- network/Wi-Fi expansion.
DOC

# Safety checks.
if grep -RInE '(^#!.*python|python3|[^A-Za-z0-9_]python[^A-Za-z0-9_])' "$JOURNAL_C" "$JOURNAL_H" 2>/dev/null; then
  echo "[v23] ERROR: unexpected Python reference in journal files" >&2
  exit 1
fi

for sym in \
  'session_journal_capture_previous' \
  'session_journal_show_summary' \
  'session_journal_recover_last' \
  'journal summary' \
  'journal recover' \
  'previous_session_captured'; do
  if ! grep -q "$sym" "$JOURNAL_C" "$JOURNAL_H" 2>/dev/null; then
    echo "[v23] ERROR: expected symbol missing after patch: $sym" >&2
    exit 1
  fi
done

echo "[v23] journal recovery and last-session summary patch applied"
