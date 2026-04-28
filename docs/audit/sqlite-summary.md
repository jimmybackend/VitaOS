# Hosted SQLite audit summary

Patch: `audit: add hosted SQLite summary command`

## Purpose

This command gives a quick console-readable summary of the hosted SQLite audit backend.

It is intended for F1A/F1B validation, smoke testing and operator review.

## Commands

```text
audit sqlite
audit sqlite summary
audit db
audit db summary
audit summary db
```

## Hosted output

The report includes:

- persistent readiness;
- current boot id;
- total boot sessions;
- total audit events;
- events for the current boot;
- hardware snapshots;
- AI proposals;
- human responses;
- node peers.

## UEFI output

UEFI reports that the SQLite summary is unavailable until the freestanding path has real persistent SQLite audit.

## Scope

This is read-only. It does not modify, repair or delete SQLite rows.
