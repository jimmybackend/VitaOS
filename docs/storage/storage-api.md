# VitaOS minimal storage writer

Status: F1A/F1B preparation layer

This document describes the first minimal VitaOS writable storage facade. It is intentionally small and does not implement a full filesystem stack or VFS.

## Purpose

The goal is to prove that VitaOS can write plain text files into the prepared `/vita/` area of the boot USB image.

This is the foundation for later features:

- note editor command
- emergency reports saved to USB
- AI prompt/response logs
- exported diagnostic files
- future SQLite audit persistence on writable media

## Paths

The storage facade only accepts paths under:

```text
/vita/
```

Examples:

```text
/vita/tmp/storage-test.txt
/vita/notes/test.txt
/vita/emergency/reports/report-0001.txt
/vita/messages/drafts/message-0001.txt
```

Paths containing `..` or `:` are rejected.

## Commands

```text
storage
storage status
storage test
storage write /vita/notes/test.txt hello from VitaOS
```

### `storage status`

Shows whether the storage facade is initialized, writable, and which backend is active.

### `storage test`

Writes:

```text
/vita/tmp/storage-test.txt
```

This is the recommended first validation on a USB image.

### `storage write <path> <text>`

Writes one line of text to a `/vita/...` path.

Example:

```text
storage write /vita/notes/hello.txt VitaOS wrote this from the console
```

## Hosted backend

In hosted builds, `/vita/...` maps to:

```text
build/storage/vita/...
```

This lets developers test the command without a physical USB.

## UEFI backend

In UEFI builds, the backend uses:

- Loaded Image Protocol
- Simple File System Protocol
- File Protocol

The writer opens the same filesystem that loaded `BOOTX64.EFI`. The parent folders must already exist. Patch 5 prepared the expected `/vita/` directory tree in the USB image builder.

## Current limitations

This patch does not add:

- general VFS
- directory creation from VitaOS in UEFI
- file append mode
- file listing
- file reading
- note editor
- SQLite persistence
- permissions
- encryption
- journaling
- malloc

## Next step

The next planned patch is:

```text
editor: add simple note editor command
```

That patch should build on `storage_write_text()` and save notes under `/vita/notes/`.
