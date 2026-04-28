# VitaOS note editor

`editor: add simple note editor command` adds the first line-oriented text editor for VitaOS F1A/F1B.

The goal is not to clone `nano` yet. The goal is to provide a safe, predictable editor that works in hosted mode and in UEFI text input without relying on Ctrl key combinations, malloc, a filesystem-wide VFS, or a GUI.

## Commands

```text
notes
note
note <file>
edit /vita/notes/<file>
```

Examples:

```text
note
note field-report
note emergency.txt
edit /vita/emergency/reports/report-001.txt
```

`note` writes inside `/vita/notes/` by default. If the filename does not contain a dot, `.txt` is appended automatically.

`edit` requires an explicit `/vita/...` path.

## Editor commands

Inside the editor prompt:

```text
.save   save current buffer
.cancel cancel editing
.show   show current buffer
.clear  clear current buffer
.help   show editor help
```

Every other line is appended to the current buffer.

## Storage backend

The editor writes using:

```c
storage_write_text(path, text)
```

Hosted maps `/vita/...` to:

```text
build/storage/vita/...
```

UEFI writes into the prepared FAT tree on the USB image using the minimal storage facade added in the previous patch.

## Current limitations

- Maximum note text: 1536 bytes.
- Existing files are overwritten.
- No file listing yet.
- No file read/edit existing content yet.
- No Ctrl+O/Ctrl+X shortcuts yet.
- No full-screen cursor editing yet.
- No scrollback editor viewport yet.

These are intentional F1A/F1B limits. The next editor step can add file reading, append mode, and a small list command for `/vita/notes`.
