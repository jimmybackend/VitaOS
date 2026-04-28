# Patch v8 — storage: add file read and notes list

This patch adds the next minimal storage step after the note editor.

## Adds

- `storage_read_text()`
- `storage_list_notes()`
- `storage read /vita/...`
- `storage cat /vita/...`
- `cat /vita/...`
- `notes list`
- editor preload of existing text when opening a file

## Apply

```bash
cd ~/VitaOS
unzip -o ~/vitaos-storage-read-notes-v8.zip -d .
python3 tools/patches/apply-storage-read-notes-v8.py
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
note test
hello from VitaOS
.save
notes list
cat /vita/notes/test.txt
shutdown
```

## Commit

```bash
git add include/vita/storage.h \
        kernel/storage.c \
        kernel/editor.c \
        kernel/command_core.c \
        kernel/console.c \
        docs/storage/storage-read-notes.md \
        tools/patches/apply-storage-read-notes-v8.py \
        README-storage-read-notes-v8.md

git commit -m "storage: add file read and notes list"
```
