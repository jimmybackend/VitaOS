# Boot self-test command

Patch: `boot: add self-test command`

## Purpose

The self-test command gives the operator a compact health report for the current VitaOS F1A/F1B boot session.

It checks and reports:

- console readiness;
- audit readiness;
- storage readiness;
- journal activity;
- hardware snapshot availability;
- proposal engine readiness;
- node core readiness;
- current mode: operational or restricted diagnostic.

## Commands

```text
selftest
self-test
boot selftest
boot self-test
checkup
```

## Output files

When writable storage is available, VitaOS writes:

```text
/vita/export/reports/self-test.txt
/vita/export/reports/self-test.jsonl
```

## Scope boundaries

This command is a local check/report. It does not repair audit data, scan the whole filesystem, upload data, or add a GUI/window manager.

UEFI can still report restricted diagnostic mode until real persistent SQLite audit exists in the freestanding path.
