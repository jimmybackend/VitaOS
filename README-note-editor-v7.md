# Patch v7 — editor: add simple note editor command

This patch adds VitaOS's first text editor path.

## New files

- `include/vita/editor.h`
- `kernel/editor.c`
- `docs/storage/note-editor.md`
- `tools/patches/apply-note-editor-v7.py`

## Modified by patch script

- `Makefile`
- `kernel/command_core.c`
- `kernel/console.c`

## New commands

```text
notes
note
note <file>
edit /vita/notes/<file>
```

Inside the editor:

```text
.save
.cancel
.show
.clear
.help
```

## Examples

```text
note
hello from VitaOS
.save
```

```text
note emergency-report
Keyboard works.
Storage writer works.
This report was written from VitaOS.
.save
```

```text
edit /vita/emergency/reports/report-001.txt
Emergency report draft.
.save
```

## Scope

This is a small F1A/F1B editor. It is line-oriented and storage-backed, not a full nano clone yet.

Not included:

- full-screen editor viewport
- file listing
- file reading existing content
- append mode
- Ctrl shortcuts
- GUI
- malloc
- broad VFS
