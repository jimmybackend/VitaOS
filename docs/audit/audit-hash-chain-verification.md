# Audit hash-chain verification

Patch: `audit: add hash chain verification command`

## Purpose

VitaOS already writes append-mostly `audit_event` records in hosted mode with a hash chain. This patch adds a visible command to verify the current boot session chain from inside the guided console.

## Commands

```text
audit verify
audit verify-chain
audit hash
audit hash-chain
```

## Hosted behavior

The verifier reads `audit_event` rows for the active `boot_id`, ordered by `event_seq`.

For each row it checks:

1. `prev_hash` equals the previous verified event hash for the same boot session.
2. `event_hash` matches the recomputed SHA-256 payload used by the current audit writer.

The report prints:

- availability;
- result;
- checked event count;
- first sequence;
- last sequence;
- first failing event if found;
- summary message.

## UEFI behavior

UEFI still has explicit audit stubs until a persistent SQLite backend exists there. In UEFI, this command reports that verification is unavailable and keeps the system honest about restricted diagnostic mode.

## Scope boundaries

This patch does not repair or rewrite audit rows. It only reports verification status.

It does not add Python, malloc in UEFI, GUI/window manager, networking, Wi-Fi, compression, encryption, or remote upload.
