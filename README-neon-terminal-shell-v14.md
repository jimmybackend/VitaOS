# VitaOS patch v14 — centered neon terminal shell

This patch adds the first visual terminal shell layer for VitaOS F1A/F1B.

## What it adds

- A centered terminal-style header drawn by the guided console.
- Cyan/blue neon-style text theme in UEFI when firmware supports Simple Text Output `SetAttribute`.
- A clean screen after splash before the guided console is printed.
- Visual title bar controls:
  - `[MIN]` minimize/collapse visual panel
  - `[RESTART]` planned safe restart control
  - `[POWER]` planned safe shutdown control
- Commands:
  - `minimize`
  - `restore`
  - `maximize`
- Documentation for the staged visual shell design.

## Current limitations

This is still text-first and F1A/F1B-safe:

- no full GUI
- no desktop/window manager
- no framebuffer font renderer yet
- no real mouse click handling yet
- no real restart/shutdown button action yet
- no malloc
- no new runtime dependencies

The controls are intentionally visual and keyboard-command driven in this patch. Pointer click handling and safe power flow are the next patches.

## Apply

```bash
cd ~/VitaOS
unzip -o ~/vitaos-neon-terminal-shell-v14.zip -d .
python3 tools/patches/apply-neon-terminal-shell-v14.py
```

## Build

```bash
make clean && \
make hosted && \
make smoke-audit && \
make && \
make iso
```

## Commit

```bash
git add include/vita/console.h \
        kernel/console.c \
        kernel/command_core.c \
        arch/x86_64/boot/uefi_entry.c \
        docs/console/neon-terminal-shell.md \
        tools/patches/apply-neon-terminal-shell-v14.py \
        README-neon-terminal-shell-v14.md

git commit -m "console: add centered neon terminal shell"
```

## Long commit description

```bash
git commit -m "console: add centered neon terminal shell" \
-m "Add the first VitaOS visual terminal shell layer for F1A/F1B.

Implemented:
- centered guided terminal header
- UEFI cyan/blue text theme using Simple Text Output SetAttribute when available
- clean screen after splash before guided console output
- visual title bar controls for MIN, RESTART, and POWER
- minimize/restore/maximize commands for the terminal panel
- documentation for the staged text-first shell design

This is not a GUI or desktop environment. Pointer click handling and safe restart/shutdown execution remain staged for follow-up patches.

Not included:
- full framebuffer renderer
- mouse click handling
- real power/restart buttons
- window manager
- malloc
- new runtime dependencies"
```
