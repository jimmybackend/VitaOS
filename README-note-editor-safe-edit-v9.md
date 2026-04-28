# Patch v9 — editor: add append mode and safer save workflow

This patch improves the minimal VitaOS note editor after the storage read/list patch.

## Added

- `append <file>` command for `/vita/notes/<file>`
- `append /vita/...` command for explicit VitaOS storage paths
- `.append` editor command
- `.replace` editor command
- `.save!` force-save command
- `.confirm` destructive-save confirmation command
- `.saveas /vita/...` command
- `.path` and `.status` editor commands
- safer save flow when an existing file was cleared or replaced

## Not included

- full nano clone
- Ctrl shortcuts
- cursor navigation inside existing text
- delete/rename
- recursive file browser
- framebuffer GUI/editor viewport
- malloc
- new runtime dependencies

## Apply

```bash
cd ~/VitaOS
unzip -o ~/vitaos-note-editor-safe-edit-v9.zip -d .
python3 tools/patches/apply-note-editor-safe-edit-v9.py
```

## Build

```bash
make clean && \
make hosted && \
make smoke-audit && \
make && \
make iso
```
