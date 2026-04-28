# Patch v12: export JSONL session report

## Summary

Adds a machine-readable JSONL session report written to:

```text
/vita/export/reports/last-session.jsonl
```

## Commands

```text
export jsonl
export session jsonl
session export jsonl
jsonl export
```

## Scope

This complements the existing plain text `export session` report.
It uses the existing storage facade and keeps the output small and freestanding-friendly.

## Not included

- SQLite dump
- full audit event export
- timestamped filenames
- compression
- encryption
- remote upload
- GUI
- malloc
- new runtime dependencies
