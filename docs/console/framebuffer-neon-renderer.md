# Framebuffer Neon Renderer

This patch adds the first GOP-backed visual renderer for the VitaOS terminal shell.

## Purpose

VitaOS remains text-first in F1A/F1B, but the boot experience now has a stronger visual identity:

- dark full-screen background;
- centered terminal panel;
- cyan/blue neon borders;
- title bar area;
- visual control boxes for minimize, restart, and power;
- prompt rail drawn near the bottom of the terminal area.

The existing guided console text still uses UEFI Simple Text Output. This renderer draws the background and panel behind it.

## Scope

This is not a GUI, desktop, window manager, or compositor. It is a framebuffer-backed terminal frame for the current text console.

## Fallback behavior

If GOP is unavailable or the firmware does not expose a usable framebuffer, VitaOS silently falls back to the existing UEFI text console.

## Current limitations

This patch does not add:

- bitmap font drawing;
- text layout inside the framebuffer;
- scrollback framebuffer repaint;
- mouse cursor drawing;
- animation;
- alpha blending;
- ACPI shutdown;
- GUI/window manager;
- malloc or new runtime dependencies.

## Next possible steps

Recommended follow-ups:

1. Add a small bitmap font renderer.
2. Move console text into framebuffer panels.
3. Add framebuffer scrollback viewport.
4. Draw mouse cursor and hover states.
