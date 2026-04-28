# VitaOS storage read and notes list

This patch completes the first small storage cycle for F1A/F1B:

1. prepare a writable `/vita` tree in the USB image;
2. write files with `storage write` or the note editor;
3. read files back with `storage read` or `cat`;
4. list saved notes with `notes list`.

## Commands

```text
storage read /vita/notes/test.txt
storage cat /vita/notes/test.txt
cat /vita/notes/test.txt
notes list
storage notes list
```

## Hosted mapping

In hosted mode, `/vita/...` maps to:

```text
build/storage/vita/...
```

## UEFI mapping

In UEFI mode, `/vita/...` maps to the FAT filesystem that contains `BOOTX64.EFI` and the prepared `/vita` tree.

## Limits

- reads are capped to a small fixed buffer;
- note listing is focused on `/vita/notes`;
- no append mode;
- no recursive directory browsing;
- no delete/rename;
- no full nano clone;
- no malloc;
- no SQLite persistence in UEFI yet.
