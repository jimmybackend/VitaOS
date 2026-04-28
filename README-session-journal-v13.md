# Patch v13 — audit: add persistent text session journal

Adds a visible persistent command/session journal before full UEFI SQLite persistence is available.

## Added

- `include/vita/session_journal.h`
- `kernel/session_journal.c`
- `docs/audit/session-journal.md`
- `tools/patches/apply-session-journal-v13.py`

## Modified by script

- `Makefile`
- `kernel/kmain.c`
- `kernel/command_core.c`
- `kernel/console.c`

## Runtime files

```text
/vita/audit/session-journal.jsonl
/vita/audit/session-journal.txt
```

These can be opened later from Linux or Windows by reading the USB FAT32 volume.

## Behavior

- initializes after storage setup;
- shows a visible journal-ready message;
- records submitted commands after Enter;
- records summarized system responses;
- redacts sensitive-looking commands;
- writes JSONL and TXT files.

Not included: hidden raw keylogger, SQLite persistence, hash chain, encryption, GUI, malloc, new dependencies.
