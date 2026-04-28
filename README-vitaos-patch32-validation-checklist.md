# VitaOS patch v32 — F1A/F1B validation checklist

Commit title:

```text
docs: add F1A/F1B validation checklist
```

## What this changes

This patch adds a repeatable validation checklist for the current VitaOS F1A/F1B base.

It documents the expected build, smoke, hosted, UEFI, storage, journal, audit, export, diagnostic and self-test checks after the v20-v31 hardening patches.

## Files

```text
docs/validation/f1a-f1b-validation-checklist.md
tools/patches/apply-f1a-f1b-validation-checklist-v32.sh
tools/patches/verify-f1a-f1b-validation-checklist-v32.sh
```

## Validate

```bash
bash tools/patches/verify-f1a-f1b-validation-checklist-v32.sh
make clean
make hosted
make smoke-audit
make
make smoke
make iso
```

## Scope

This is documentation and validation discipline only.

Not included:

- UEFI SQLite persistence;
- new kernel features;
- Python runtime or Python build dependency;
- malloc;
- GUI/window manager;
- network or Wi-Fi expansion.
