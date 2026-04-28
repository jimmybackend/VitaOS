# VitaOS patch v22 — storage tree validator and repair command

Commit title:

```text
storage: add USB tree validator and repair command
```

## What this patch adds

This patch adds a small, explicit validator/repair layer for the prepared VitaOS writable USB tree.

Commands added:

```text
storage tree
storage check
storage repair
```

Aliases added:

```text
storage paths
storage validate
storage verify
storage init-tree
storage mkdirs
```

The patch stays inside F1A/F1B scope:

- C/freestanding-friendly runtime changes;
- no Python inside VitaOS or mandatory build paths;
- no malloc;
- no GUI/window manager;
- no full VFS;
- no network/Wi-Fi expansion.

## Apply

```bash
cd VitaOS
git pull
unzip -o /ruta/vitaos-patch22-storage-tree-validator.zip -d .
bash tools/patches/apply-storage-tree-validator-v22.sh
```

## Validate

```bash
bash tools/patches/verify-storage-tree-v22.sh
make clean
make hosted
make smoke-audit
make
make smoke
make iso
```

## Manual test

Hosted:

```bash
./build/hosted/vitaos-hosted
```

Inside VitaOS:

```text
storage tree
storage check
storage repair
storage check
storage status
note test.txt
.save
notes list
export session
journal status
shutdown
```

UEFI:

```text
storage tree
storage check
storage repair
storage test
journal flush
export session
shutdown
```

## Commit

```bash
git status
git diff -- kernel/storage.c include/vita/storage.h docs/storage/tree-validator.md

git add kernel/storage.c \
  include/vita/storage.h \
  docs/storage/tree-validator.md \
  tools/patches/apply-storage-tree-validator-v22.sh \
  tools/patches/verify-storage-tree-v22.sh \
  README-vitaos-patch22-storage-tree.md

git commit -m "storage: add USB tree validator and repair command"
git push
```

## Long commit description

```text
storage: add USB tree validator and repair command

Add an explicit validator and repair flow for the prepared VitaOS writable USB tree.

The current F1A/F1B storage model intentionally restricts file operations to /vita/ and uses hosted filesystem mapping or UEFI Simple File System rather than a full VFS. This patch makes the expected writable tree visible and repairable before journal, notes, messages, emergency reports, AI session files, export files and temporary files depend on it.

Implemented:
- storage tree
- storage check
- storage repair
- aliases: storage paths, storage validate, storage verify, storage init-tree, storage mkdirs
- hosted directory check/create under build/storage/vita/
- UEFI Simple File System directory check/create
- public storage_show_tree(), storage_check_tree(), storage_repair_tree()
- documentation for the expected /vita/ tree
- Bash verifier for v22 patch presence

This keeps VitaOS text-first, audit-first and F1A/F1B scoped.

Not included:
- Python runtime or Python build dependency
- malloc
- GUI/window manager
- full filesystem browser
- broad VFS
- recursive delete
- FAT formatting
- SQLite UEFI persistence
- network or Wi-Fi expansion
```
