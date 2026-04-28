# Patch v18 — console: add framebuffer bitmap font renderer

This patch adds a UEFI GOP-backed bitmap text renderer for the VitaOS centered neon terminal shell.

## Apply

```bash
cd ~/VitaOS
unzip -o ~/vitaos-framebuffer-bitmap-font-v18.zip -d .
python3 tools/patches/apply-framebuffer-bitmap-font-v18.py
```

## Build

```bash
make clean && \
make hosted && \
make smoke-audit && \
make && \
make iso
```

## Test commands

Inside VitaOS:

```text
clear
status
hw
journal status
notes list
export session
export jsonl
shutdown
```

## What this does

- Adds `uefi_neon_text.c/.h`.
- Adds a tiny built-in 5x7 bitmap font.
- Renders console output inside the neon panel when GOP is available.
- Keeps fallback to the existing UEFI text console.
- Keeps the milestone text-first and freestanding-friendly.

## What this does not do yet

- No full scrollback viewport.
- No PageUp/PageDown.
- No mouse cursor drawing.
- No GUI/window manager.
- No malloc.
- No new runtime dependencies.
