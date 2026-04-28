# Audit readiness gate

Patch: `audit: add UEFI audit readiness gate report`

## Purpose

VitaOS is audit-first. Full operational mode must not be claimed unless a persistent audit backend is active.

This patch adds a visible readiness report so the operator can distinguish:

- hosted SQLite audit ready;
- UEFI restricted diagnostic mode while persistent SQLite audit is still pending.

## Commands

```text
audit
audit status
audit report
audit gate
audit readiness
```

## Expected behavior

Hosted mode may report:

```text
persistent_ready: yes
backend: hosted_sqlite
mode: OPERATIONAL
audit_state: READY
```

UEFI mode currently reports:

```text
persistent_ready: no
restricted_diagnostic: yes
backend: uefi_stub
mode: RESTRICTED_DIAGNOSTIC
audit_state: FAILED
```

This is intentional until a real persistent SQLite backend is implemented for UEFI.

## Scope boundaries

Included:

- central audit readiness status;
- visible bilingual console report;
- hosted/UEFI distinction;
- explicit gate wording.

Not included:

- UEFI SQLite persistence;
- hash-chain verification command;
- Python;
- malloc;
- GUI/window manager;
- network or Wi-Fi expansion.
