# VitaOS patch v10 — storage delete and rename commands

This patch adds the next minimal storage-management step for VitaOS F1A/F1B.

It adds safe file deletion and rename/copy-move behavior for paths under `/vita/` only.
The implementation is intentionally small and text-first:

- no full VFS
- no recursive directory operations
- no wildcard support
- no deletion outside `/vita/`
- no deletion of protected VitaOS root directories
- no GUI
- no malloc
- no new runtime dependencies

## New commands

```text
storage delete /vita/notes/file.txt
storage rm /vita/notes/file.txt
rm /vita/notes/file.txt
delete /vita/notes/file.txt

storage rename /vita/notes/old.txt /vita/notes/new.txt
storage mv /vita/notes/old.txt /vita/notes/new.txt
rename /vita/notes/old.txt /vita/notes/new.txt
mv /vita/notes/old.txt /vita/notes/new.txt
```

## Behavior

`delete` removes a single file under `/vita/`.

`rename` uses the current minimal storage API by reading the source text, writing the destination text, then deleting the source. This keeps the UEFI implementation simple and avoids adding a full FAT rename implementation yet.

Because the current read buffer is limited, rename is intended for small text notes and reports.

## Apply

```bash
cd ~/VitaOS
unzip -o ~/vitaos-storage-delete-rename-v10.zip -d .
python3 tools/patches/apply-storage-delete-rename-v10.py
```

## Build

```bash
make clean && \
make hosted && \
make smoke-audit && \
make && \
make iso
```

## Hosted test

```text
note old
hello from VitaOS
.save
cat /vita/notes/old.txt
rename /vita/notes/old.txt /vita/notes/new.txt
cat /vita/notes/new.txt
rm /vita/notes/new.txt
notes list
shutdown
```

## Commit

```bash
git add include/vita/storage.h \
        kernel/storage.c \
        kernel/command_core.c \
        kernel/console.c \
        docs/storage/storage-delete-rename.md \
        tools/patches/apply-storage-delete-rename-v10.py \
        README-storage-delete-rename-v10.md

git commit -m "storage: add delete and rename commands"
```
