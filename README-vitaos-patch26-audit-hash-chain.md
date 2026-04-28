# VitaOS patch v26 — audit hash-chain verification

Commit title:

```text
audit: add hash chain verification command
```

## What this changes

This patch adds a visible audit hash-chain verification command for the current boot session.

New commands:

```text
audit verify
audit verify-chain
audit hash
audit hash-chain
```

## Scope

Hosted:
- verifies `audit_event` rows for the current `boot_id`;
- recomputes each event hash with the existing SHA-256 path;
- verifies each `prev_hash` points to the previous event hash in the same boot session;
- reports checked event count, first/last sequence and first failure if any.

UEFI:
- reports that hash-chain verification is unavailable until the persistent SQLite audit backend exists there;
- remains in restricted diagnostic behavior.

## Validate

```bash
bash tools/patches/verify-audit-hash-chain-v26.sh

make clean
make hosted
make smoke-audit
make
make smoke
make iso
```

## Manual test

```text
audit verify
audit hash-chain
status
journal status
diagnostic
shutdown
```

## Not included

- UEFI SQLite persistence;
- full historical multi-boot database validation;
- repair/mutation of audit rows;
- Python runtime or Python build dependency;
- malloc in freestanding/UEFI path;
- GUI/window manager;
- network or Wi-Fi expansion.
