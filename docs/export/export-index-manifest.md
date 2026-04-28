# Export index manifest

Patch: `storage: add export index manifest`

## Purpose

VitaOS now has several local export files under `/vita/export/` and visible journal files under `/vita/audit/`.

This patch adds a bounded manifest command so the operator can create and inspect an index of known export files without remembering every path.

## Commands

```text
export index
export manifest
export list
exports
storage export-index
```

## Output files

```text
/vita/export/export-index.txt
/vita/export/export-index.jsonl
```

## Behavior

The manifest checks known VitaOS report paths and records whether each one is currently readable.

It does not recursively scan the disk and does not expose a broad filesystem browser.

## Known indexed paths

```text
/vita/export/reports/last-session.txt
/vita/export/reports/last-session.jsonl
/vita/export/reports/diagnostic-bundle.txt
/vita/export/reports/diagnostic-bundle.jsonl
/vita/export/audit/audit-verify.txt
/vita/export/audit/audit-verify.jsonl
/vita/export/audit/current-session-events.txt
/vita/export/audit/current-session-events.jsonl
/vita/audit/session-journal.txt
/vita/audit/session-journal.jsonl
```

## Scope boundaries

Not included:

- recursive filesystem browser;
- wildcard scans;
- compression/encryption;
- remote upload;
- Python;
- malloc;
- GUI/window manager;
- networking or Wi-Fi expansion.
