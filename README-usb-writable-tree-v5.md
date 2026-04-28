# Patch 5: usb: prepare VitaOS writable directory tree

This patch prepares the VitaOS USB image with a predictable `/vita/` writable data tree.

## Files changed

```text
tools/image/make-uefi-usb-image.sh
docs/storage/usb-layout.md
```

## What this patch does

The USB image builder now creates these directories inside the FAT32 image:

```text
/vita/config/
/vita/audit/
/vita/notes/
/vita/messages/inbox/
/vita/messages/outbox/
/vita/messages/drafts/
/vita/emergency/reports/
/vita/emergency/checklists/
/vita/ai/prompts/
/vita/ai/responses/
/vita/ai/sessions/
/vita/net/profiles/
/vita/export/audit/
/vita/export/notes/
/vita/export/reports/
/vita/tmp/
```

It also copies seed files:

```text
/vita/README.txt
/vita/config/vitaos.cfg
/vita/config/network.cfg
/vita/config/ai.cfg
/vita/notes/README.txt
/vita/messages/README.txt
/vita/emergency/README.txt
```

## What this patch does not do

This patch does not add:

```text
- UEFI file writer from the kernel
- note editor
- SQLite UEFI persistence
- VFS
- network credential storage
- GUI
- malloc
- new runtime dependencies
```

## Build and test

```bash
make clean
make
./tools/image/make-uefi-usb-image.sh build/image/vitaos-uefi-live.img
```

Optional verification:

```bash
MTOOLS_SKIP_CHECK=1 mdir -i build/image/vitaos-uefi-live.img@@1048576 ::/vita
MTOOLS_SKIP_CHECK=1 mdir -i build/image/vitaos-uefi-live.img@@1048576 ::/vita/config
MTOOLS_SKIP_CHECK=1 mdir -i build/image/vitaos-uefi-live.img@@1048576 ::/vita/notes
```

## Commit

```bash
git add tools/image/make-uefi-usb-image.sh docs/storage/usb-layout.md README-usb-writable-tree-v5.md
git commit -m "usb: prepare VitaOS writable directory tree"
```

Long description:

```bash
git commit -m "usb: prepare VitaOS writable directory tree" \
-m "Prepare the USB image with a predictable /vita writable data area for future audit persistence, notes, messages, emergency reports, AI gateway sessions, network profiles, exports, and temporary files.

The image builder now creates the /vita directory tree and seeds README/config placeholder files inside the FAT32 USB image using mtools, without loop mounts or sudo.

This is a storage preparation patch only. It does not add a kernel file writer, note editor, UEFI SQLite persistence, VFS, network credential handling, GUI, malloc, or new runtime dependencies."
```
