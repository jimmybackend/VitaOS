# VitaOS patch v28 — export current session audit events

Commit title:

```text
audit: export current session events
```

## What this changes

This patch exports the current boot session's audit events into the prepared VitaOS writable tree:

```text
/vita/export/audit/current-session-events.txt
/vita/export/audit/current-session-events.jsonl
```

## Commands

```text
audit events
audit export events
audit events export
export audit events
export current audit
```

## Scope

This is a report/export step only. It does not repair, rewrite, delete, or mutate audit rows.

It remains F1A/F1B scoped:

- no Python runtime;
- no malloc;
- no GUI/window manager;
- no network or Wi-Fi expansion;
- no UEFI SQLite persistence yet.

## Validate

```bash
bash tools/patches/verify-audit-events-export-v28.sh
make clean
make hosted
make smoke-audit
make
make smoke
make iso
```
