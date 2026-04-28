# Mouse wheel scroll and visual scroll indicator

Patch: `console: add mouse wheel and visual scroll indicator`

## Behavior

The UEFI neon framebuffer terminal keeps the existing RAM scrollback and adds optional pointer wheel scrolling.

Supported input paths:

- `PageUp`: scrolls up through recent framebuffer terminal output.
- `PageDown`: scrolls down toward live output.
- Simple Pointer Protocol: uses `RelativeMovementZ` when firmware reports wheel-style movement.
- Absolute Pointer Protocol: uses `CurrentZ` deltas when firmware reports compatible Z movement.

If no pointer protocol is present, or if the pointer does not expose wheel-compatible Z movement, VitaOS continues normally and shows `wheel:n/a` inside the panel.

## Scope

This is still text-first F1A/F1B. It is not a GUI, desktop, compositor, or window manager.

Not included:

- mouse cursor drawing;
- draggable windows;
- desktop widgets;
- real Wi-Fi/network expansion;
- persistent framebuffer scrollback;
- Python;
- malloc;
- new OS dependencies.

The scrollback remains RAM-only. Persistent command/event history remains the visible session journal under `/vita/audit/`.
