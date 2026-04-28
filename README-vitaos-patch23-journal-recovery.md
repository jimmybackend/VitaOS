# VitaOS patch v23 — journal boot recovery and last-session summary

Commit title:

```text
journal: add boot recovery and last-session summary
```

## What this changes

Adds a bounded, visible recovery path for the previous `/vita/audit/session-journal.txt` before the current boot starts writing the new session journal.

Commands added:

```text
journal summary
journal last-session
journal last
journal recover
```

## Apply

```bash
bash tools/patches/apply-journal-recovery-summary-v23.sh
```

## Validate

```bash
bash tools/patches/verify-journal-recovery-summary-v23.sh
make clean
make hosted
make smoke-audit
make
make smoke
make iso
```

## Manual test

```text
journal status
journal summary
journal note prueba de recuperacion
journal flush
journal last
journal recover
shutdown
```

Boot again and run:

```text
journal summary
journal last
journal recover
```

## Scope boundaries

This patch keeps VitaOS F1A/F1B text-first and audit-first.

Not included:

- Python runtime or Python build dependency;
- malloc;
- GUI/window manager;
- SQLite UEFI persistence;
- hash-chain verification;
- network/Wi-Fi expansion.
