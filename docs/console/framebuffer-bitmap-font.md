# VitaOS framebuffer bitmap font renderer

Patch: `console: add framebuffer bitmap font renderer`

This patch adds the first UEFI GOP-backed bitmap text renderer for the VitaOS neon terminal shell.

## Purpose

The previous neon framebuffer patch draws the centered terminal frame, title bar, and control boxes. This patch adds a tiny built-in 5x7 bitmap font so console output can be rendered inside the neon panel instead of relying only on UEFI Simple Text Output.

The renderer is still text-first and F1A/F1B scoped. It is not a desktop, window manager, compositor, or full GUI.

## Files

- `arch/x86_64/boot/uefi_neon_text.h`
- `arch/x86_64/boot/uefi_neon_text.c`
- `arch/x86_64/boot/uefi_entry.c`
- `Makefile`

## Behavior

When UEFI GOP is available:

- screen is cleared to a dark background;
- a centered cyan/blue panel is drawn;
- title bar and visual controls are drawn;
- console lines are rendered using the built-in bitmap font;
- prompt and command echo are rendered inside the panel;
- output wraps and clears the body area when the panel is full.

When GOP is not available:

- VitaOS silently falls back to the existing UEFI text console.

## Current limitations

This is the first framebuffer text pass. It does not yet include:

- full scrollback repaint;
- PageUp/PageDown viewport;
- mouse cursor drawing;
- proportional fonts;
- UTF-8 or accents;
- anti-aliasing;
- animations;
- GUI/window manager;
- malloc or dynamic font loading.

The built-in font covers uppercase/lowercase ASCII by drawing lowercase as uppercase, digits, and common punctuation used by the current console commands.

## Next recommended step

`console: add framebuffer scrollback viewport`

That future patch should add a small line buffer and redraw the visible viewport instead of clearing the output area when it fills.
