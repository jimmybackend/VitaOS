# VitaOS patch v30 — export index manifest

Commit title:

```text
storage: add export index manifest
```

## What this changes

This patch adds a small export manifest/index command for the prepared VitaOS writable tree.

It writes:

```text
/vita/export/export-index.txt
/vita/export/export-index.jsonl
```

## Commands

```text
export index
export manifest
export list
exports
storage export-index
```

## Scope

This is not a full filesystem browser. It checks known VitaOS export and journal paths and records whether each known file is currently readable.

It remains F1A/F1B scoped:

- no Python runtime;
- no malloc;
- no GUI/window manager;
- no recursive filesystem browser;
- no compression/encryption;
- no remote upload;
- no network or Wi-Fi expansion.

## Validate

```bash
bash tools/patches/verify-export-index-manifest-v30.sh
make clean
make hosted
make smoke-audit
make
make smoke
make iso
```
