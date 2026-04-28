# VitaOS patch v29 — hosted SQLite audit summary

Commit title:

```text
audit: add hosted SQLite summary command
```

## What this changes

This patch adds a console-visible summary for the hosted SQLite audit backend.

It does not replace `audit verify` or event export. It gives the operator a quick status/count view of the current SQLite database without opening `sqlite3` manually.

## Commands

```text
audit sqlite
audit sqlite summary
audit db
audit db summary
audit summary db
```

## Hosted behavior

Hosted mode reports counts from:

```text
boot_session
audit_event
hardware_snapshot
ai_proposal
human_response
node_peer
```

It also reports the current boot id and the number of audit events for the active boot session.

## UEFI behavior

UEFI reports that SQLite summary is unavailable until the freestanding path has real persistent SQLite audit.

## Validate

```bash
bash tools/patches/verify-sqlite-summary-v29.sh
make clean
make hosted
make smoke-audit
make
make smoke
make iso
```

## Scope boundaries

Not included:

- UEFI SQLite persistence;
- schema migrations beyond current existing schema;
- database repair;
- Python runtime or Python build dependency;
- malloc in UEFI/freestanding path;
- GUI/window manager;
- network or Wi-Fi expansion.
