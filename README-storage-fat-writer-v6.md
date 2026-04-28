# Patch v6 — storage: add minimal UEFI FAT file writer

This patch adds the first minimal VitaOS writable storage facade.

## Files added

```text
include/vita/storage.h
kernel/storage.c
docs/storage/storage-api.md
tools/patches/apply-storage-fat-writer-v6.py
README-storage-fat-writer-v6.md
```

## Files patched by script

```text
Makefile
kernel/kmain.c
kernel/command_core.c
kernel/console.c
```

## Commands added

```text
storage
storage status
storage test
storage write /vita/notes/test.txt hello from VitaOS
```

## Behavior

Hosted:

- maps `/vita/...` to `build/storage/vita/...`
- writes text with the host filesystem
- creates parent directories as needed

UEFI:

- writes to the same FAT filesystem that loaded `BOOTX64.EFI`
- uses UEFI Simple File System + File Protocol
- expects the `/vita/` directory tree prepared by patch v5

## Not included

```text
no note editor yet
no file listing
no file read command
no full VFS
no SQLite persistence in UEFI yet
no append mode
no network storage
no GUI
no malloc
no new runtime dependencies
```

## Apply

```bash
cd ~/VitaOS
unzip -o ~/vitaos-storage-fat-writer-v6.zip -d .
python3 tools/patches/apply-storage-fat-writer-v6.py
```

## Build

```bash
make clean && \
make hosted && \
make smoke-audit && \
make && \
make iso
```

For USB image testing:

```bash
./tools/image/make-uefi-usb-image.sh build/image/vitaos-uefi-live.img
```

## Hosted test

```bash
./build/hosted/vitaos-hosted
```

Then inside VitaOS:

```text
storage status
storage test
storage write /vita/notes/hello.txt hello from hosted VitaOS
shutdown
```

Verify:

```bash
cat build/storage/vita/tmp/storage-test.txt
cat build/storage/vita/notes/hello.txt
```

## UEFI/USB test

Boot from the USB image or physical USB, then run:

```text
storage status
storage test
storage write /vita/notes/hello.txt hello from UEFI VitaOS
```

Then inspect the USB from another OS and check:

```text
/vita/tmp/storage-test.txt
/vita/notes/hello.txt
```

## Commit

```bash
git add include/vita/storage.h \
        kernel/storage.c \
        kernel/kmain.c \
        kernel/command_core.c \
        kernel/console.c \
        Makefile \
        docs/storage/storage-api.md \
        tools/patches/apply-storage-fat-writer-v6.py \
        README-storage-fat-writer-v6.md

git commit -m "storage: add minimal UEFI FAT file writer"
```
