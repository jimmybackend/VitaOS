# Centered Neon Terminal Shell

VitaOS remains a text-first F1A/F1B system, but it now has a visual terminal shell identity.

## Purpose

The neon terminal shell gives VitaOS a cleaner first-use experience without turning the project into a GUI or desktop environment.

It provides:

- a centered terminal-style header;
- a cyan/blue UEFI text theme when firmware supports it;
- a clear separation between splash and console;
- a visual title bar with staged controls;
- keyboard commands for minimize and restore.

## Visual controls

The title bar shows:

```text
[MIN] [RESTART] [POWER]
```

Current behavior:

- `[MIN]` is represented by the `minimize` command.
- `restore` or `maximize` restores the terminal panel.
- `[RESTART]` and `[POWER]` are visual placeholders for the next safe power-flow patch.

Future behavior:

- pointer click handling will allow selecting these controls with a UEFI pointer/mouse protocol when available;
- restart and shutdown will flush journal/storage/editor/AI state before calling UEFI reset services.

## Commands

```text
minimize
restore
maximize
```

## Scope

This patch intentionally does not add:

- desktop GUI;
- mouse click dispatcher;
- full framebuffer font renderer;
- compositing/window manager;
- memory allocation;
- new runtime dependencies.

## Next patches

Recommended order:

1. `console: add pointer input and title bar controls`
2. `power: add safe restart and shutdown flow`
3. `console: add framebuffer text renderer`
