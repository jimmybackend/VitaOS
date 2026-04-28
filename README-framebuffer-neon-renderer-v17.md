# VitaOS patch v17 — framebuffer neon renderer

This patch adds a GOP-backed visual frame behind the centered VitaOS terminal shell.

## What it adds

- New UEFI renderer files:
  - `arch/x86_64/boot/uefi_neon_frame.h`
  - `arch/x86_64/boot/uefi_neon_frame.c`
- New console frame renderer callback.
- UEFI binding from the console shell to the GOP renderer.
- Dark full-screen background.
- Centered neon/cyan terminal panel.
- Visual title bar and prompt rail.
- Visual MIN / RESTART / POWER boxes.
- Fallback to text console if GOP is unavailable.

## What it does not add yet

- Full framebuffer text rendering.
- Mouse cursor drawing.
- Scrollback repaint.
- GUI/window manager.
- Animation.
- malloc.
- New runtime dependencies.

## Apply

```bash
cd ~/VitaOS
unzip -o ~/vitaos-framebuffer-neon-renderer-v17.zip -d .
python3 tools/patches/apply-framebuffer-neon-renderer-v17.py
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
        arch/x86_64/boot/uefi_entry.c \
        arch/x86_64/boot/uefi_neon_frame.h \
        arch/x86_64/boot/uefi_neon_frame.c \
        Makefile \
        docs/console/framebuffer-neon-renderer.md \
        tools/patches/apply-framebuffer-neon-renderer-v17.py \
        README-framebuffer-neon-renderer-v17.md

git commit -m "console: add framebuffer neon renderer"
```

## Long commit description

```bash
git commit -m "console: add framebuffer neon renderer" \
-m "Add the first GOP-backed framebuffer renderer for the VitaOS centered neon terminal shell.

Implemented:
- UEFI GOP neon frame renderer
- dark full-screen background
- centered cyan/blue terminal panel
- title bar and visual control boxes
- prompt rail behind the text console
- console frame renderer callback
- fallback to existing text console when GOP is unavailable
- documentation for the staged renderer design

This keeps VitaOS text-first while giving the shell a stronger visual identity.

Not included:
- full framebuffer text renderer
- scrollback repaint
- mouse cursor drawing
- GUI/window manager
- animation
- malloc
- new runtime dependencies"
```
