# Audit verification export

Patch: `audit: export hash-chain verification report`

## Purpose

VitaOS already has a console-visible audit hash-chain verifier in v26.

This patch adds a persistent export for that verification result so the operator can inspect it later from the writable USB tree.

## Commands

```text
audit export
audit verify export
audit export verify
export audit
export audit verify
```

## Output files

```text
/vita/export/audit/audit-verify.txt
/vita/export/audit/audit-verify.jsonl
```

## Hosted behavior

Hosted mode can verify the current boot's SQLite `audit_event` chain and export the result.

## UEFI behavior

UEFI exports the honest diagnostic status: hash-chain verification remains unavailable until the freestanding UEFI path has real persistent SQLite audit.

## Scope boundaries

This patch does not mutate audit rows. It reports and exports only.

Not included:

- UEFI SQLite persistence;
- multi-boot historical database verification;
- row repair;
- Python;
- malloc;
- GUI/window manager;
- networking or Wi-Fi expansion.
