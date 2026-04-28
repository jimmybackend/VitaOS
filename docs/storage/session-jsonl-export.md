# VitaOS session JSONL export

Patch: `export: add JSONL session export`

This document describes the small machine-readable session checkpoint written by VitaOS.

## Command

```text
export jsonl
export session jsonl
session export jsonl
jsonl export
```

## Output path

```text
/vita/export/reports/last-session.jsonl
```

## Purpose

The JSONL export is intended for tools, later audit importers, and human review from another machine.
It complements the plain text session summary and is not a replacement for the future SQLite audit dump.

## Format

Each line is one JSON object:

```jsonl
{"type":"session","version":"f1a-f1b-session-jsonl-v1","boot_mode":"uefi"}
{"type":"hardware","available":true,"net_count":1}
{"type":"storage","initialized":true,"writable":true}
```

Current record types:

- `session`
- `hardware`
- `storage`
- `network`
- `ai_gateway`
- `paths`
- `limitations`

## Current limitations

This is intentionally small and F1A/F1B-safe.

It does not provide:

- SQLite dump
- full audit event export
- compression
- encryption
- remote upload
- timestamped filenames
- recursive notes/messages export
- GUI
- malloc-based dynamic output
