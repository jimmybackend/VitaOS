# VitaOS patch v27 — export audit verification report

Commit title:

```text
audit: export hash-chain verification report
```

## What this changes

This patch builds on v26 (`audit verify`) and adds an exportable report under the prepared VitaOS writable tree:

```text
/vita/export/audit/audit-verify.txt
/vita/export/audit/audit-verify.jsonl
```

## Commands

```text
audit export
audit verify export
audit export verify
export audit
export audit verify
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
bash tools/patches/verify-audit-export-v27.sh
make clean
make hosted
make smoke-audit
make
make smoke
make iso
```
