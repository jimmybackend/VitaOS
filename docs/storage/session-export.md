# VitaOS session export

Patch: `export: add session summary report`

This document describes the first minimal session export command for VitaOS F1A/F1B.

## Purpose

The session export command writes a plain text checkpoint of the current VitaOS session into the prepared `/vita` USB data tree.

The report is intended for:

- post-boot review from another computer;
- debugging hardware and storage state;
- keeping a human-readable emergency/session checkpoint;
- preparing future audit export and remote AI workflows.

## Command

```text
export
export session
export report
session export
```

The command writes:

```text
/vita/export/reports/last-session.txt
```

## Report content

The report includes:

- boot mode: hosted or UEFI;
- architecture;
- console readiness;
- audit readiness;
- restricted diagnostic state;
- proposal engine readiness;
- node core readiness;
- peer and proposal counters;
- hardware snapshot counters;
- storage backend status;
- network readiness summary;
- AI gateway stage summary;
- notes/messages/emergency path hints;
- current limitations.

## Current limitations

This is intentionally simple and safe:

- no timestamped filenames yet;
- no JSON export yet;
- no SQLite dump yet;
- no recursive directory archive;
- no compression;
- no encryption;
- no remote upload;
- no malloc;
- no new runtime dependencies.

The file is overwritten each time as `/vita/export/reports/last-session.txt`.

A later patch can add numbered reports once RTC/time and a stronger naming policy are available.
