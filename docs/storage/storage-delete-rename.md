# VitaOS storage delete and rename commands

This document describes the F1A/F1B minimal file administration commands for the prepared `/vita/` USB data tree.

## Scope

The commands are intentionally limited to the VitaOS writable data tree:

```text
/vita/
```

They are meant for notes, emergency reports, message drafts, AI prompts/responses, network profiles, exports, and temporary files.

They are not a general filesystem shell.

## Commands

### Delete a file

```text
rm /vita/notes/test.txt
delete /vita/notes/test.txt
storage rm /vita/notes/test.txt
storage delete /vita/notes/test.txt
```

### Rename or move a small text file

```text
mv /vita/notes/old.txt /vita/notes/new.txt
rename /vita/notes/old.txt /vita/notes/new.txt
storage mv /vita/notes/old.txt /vita/notes/new.txt
storage rename /vita/notes/old.txt /vita/notes/new.txt
```

## Safety rules

The storage layer rejects paths that:

- do not start with `/vita/`
- contain `..`
- contain `:`
- contain control characters

The delete/rename flow also protects important VitaOS directory roots such as:

```text
/vita/config
/vita/audit
/vita/notes
/vita/messages
/vita/emergency
/vita/ai
/vita/net
/vita/export
/vita/tmp
```

This prevents accidental deletion of the main tree structure prepared by the USB image builder.

## Current rename implementation

For portability across hosted and UEFI Simple File System, rename currently works as:

```text
read old file
write new file
delete old file
```

This is enough for small text notes and reports. It is not intended for large binary files.

## Limitations

Not included yet:

- recursive directory deletion
- wildcard deletion
- directory rename
- binary-safe large file rename
- full VFS
- permissions model
- journaling
- trash/recycle folder
- undelete
