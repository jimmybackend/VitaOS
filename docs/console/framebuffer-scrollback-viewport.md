# VitaOS framebuffer scrollback viewport

Patch: `console: add framebuffer scrollback viewport`

This patch extends the UEFI GOP neon terminal text renderer with a small in-memory scrollback viewport.

## Purpose

The previous framebuffer text renderer could draw text inside the centered neon panel, but long output eventually cleared/restarted the body area. This patch keeps recent completed lines in a static ring buffer and lets the operator inspect previous output.

## Controls

- `PageUp`: scroll up through recent framebuffer terminal output.
- `PageDown`: scroll down toward the live prompt/output.
- `clear`: clears the viewport and resets the visible terminal body.

The normal UEFI text-console fallback remains available when GOP is unavailable.

## Scope

Included:

- static scrollback ring buffer;
- no heap allocation;
- viewport repaint inside the neon panel;
- PageUp/PageDown handling in UEFI input loop;
- fallback-safe behavior.

Not included:

- mouse-wheel scrolling;
- scrollbar drawing;
- full terminal emulator;
- ANSI escape parsing;
- persistent scrollback on USB;
- GUI/window manager;
- malloc or external dependencies.

## Notes

The buffer is intentionally small and RAM-only for F1A/F1B. Persistent command/event history is handled separately by the visible `/vita/audit/session-journal.*` files.
