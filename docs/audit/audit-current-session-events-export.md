# Current session audit events export

Patch: `audit: export current session events`

## Purpose

This patch exports the current boot session's `audit_event` rows into the prepared VitaOS writable tree.

It complements:

- `audit verify` / `audit hash-chain`, which verifies the current session hash chain;
- `audit export`, which exports the verification result;
- `diagnostic`, which exports a high-level system bundle.

## Commands

```text
audit events
audit export events
audit events export
export audit events
export current audit
```

## Output files

```text
/vita/export/audit/current-session-events.txt
/vita/export/audit/current-session-events.jsonl
```

## Hosted behavior

Hosted mode exports the active boot session's SQLite `audit_event` rows ordered by `event_seq`.

## UEFI behavior

UEFI writes an honest unavailable report until the freestanding path has real persistent SQLite audit.

## Scope boundaries

This patch is report/export only.

Not included:

- UEFI SQLite persistence;
- full historical multi-boot export;
- audit row repair or mutation;
- compression or encryption;
- remote upload;
- Python runtime;
- malloc;
- GUI/window manager;
- networking or Wi-Fi expansion.
