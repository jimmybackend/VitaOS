# VitaOS patch v25 — diagnostic bundle export

Commit title:

```text
export: add full diagnostic bundle
```

## What this changes

Adds a small F1A/F1B diagnostic bundle command that exports a bounded text and JSONL report into:

```text
/vita/export/reports/diagnostic-bundle.txt
/vita/export/reports/diagnostic-bundle.jsonl
```

Commands:

```text
diagnostic
diag
diagnostic bundle
diagnostic export
export diagnostic
export diagnostics
export bundle
export all
```

The bundle summarizes boot mode, audit readiness, storage readiness, journal status, hardware snapshot counts, proposal/node readiness, and current known limitations.

## Apply

```bash
unzip -o vitaos-patch25-diagnostic-bundle.zip -d .
bash tools/patches/apply-diagnostic-bundle-v25.sh
```

## Validate

```bash
bash tools/patches/verify-diagnostic-bundle-v25.sh
make clean
make hosted
make smoke-audit
make
make smoke
make iso
```

## Scope boundaries

This keeps VitaOS text-first and F1A/F1B scoped.

Not included:

- Python runtime or Python build dependency;
- malloc;
- GUI/window manager;
- compression/encryption;
- remote upload;
- new network or Wi-Fi behavior;
- full SQLite dump;
- full filesystem browser.
