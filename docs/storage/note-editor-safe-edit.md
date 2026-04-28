# VitaOS note editor safe edit workflow

This document describes the v9 note editor workflow for the F1A/F1B storage slice.

## Scope

The editor remains a minimal line-oriented console editor. It is intentionally not a full nano clone yet.
It works through the existing `storage_write_text()` and `storage_read_text()` APIs.

## Commands

From the VitaOS prompt:

```text
notes
notes list
note
note field.txt
append field.txt
edit /vita/notes/field.txt
append /vita/notes/field.txt
```

Inside the editor:

```text
.save        save the current buffer
.save!       force save after a destructive change
.confirm     confirm a destructive save before using .save
.saveas PATH save the current buffer to another /vita/... path
.cancel      cancel without saving
.show        show the current buffer
.clear       clear current text and mark the session destructive
.replace     clear current text and switch to replace mode
.append      switch to append mode
.path        show target path
.status      show editor state
.help        show editor help
```

## Safety behavior

If an existing file is loaded and the user clears/replaces the buffer, the editor marks the session as a destructive change.
A normal `.save` will refuse until the user runs `.confirm`, or the user can explicitly force with `.save!`.

This prevents accidental loss of an existing note while keeping the workflow simple enough for UEFI text input.

## Current limitations

- No full-screen editor viewport yet.
- No cursor movement inside a line.
- No Ctrl shortcuts.
- No delete/rename.
- No recursive file browser.
- No malloc.
- No general VFS.
