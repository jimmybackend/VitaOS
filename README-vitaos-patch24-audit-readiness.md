# VitaOS patch v24 — audit readiness gate report

Commit title:

```text
audit: add UEFI audit readiness gate report
```

## What this changes

Adds a central audit readiness report and uses it from the `audit` command.

New aliases:

```text
audit status
audit report
audit gate
audit readiness
```

Hosted reports persistent SQLite audit readiness when the SQLite backend is active.

UEFI reports restricted diagnostic mode explicitly until a real persistent SQLite audit backend exists there.

## Apply

```bash
bash tools/patches/apply-audit-readiness-gate-v24.sh
```

## Validate

```bash
bash tools/patches/verify-audit-readiness-gate-v24.sh
make clean
make hosted
make smoke-audit
make
make smoke
make iso
```

## Scope

No Python, no malloc, no GUI/window manager, no network/Wi-Fi expansion.
