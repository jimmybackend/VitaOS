# VitaOS pointer title bar controls

Patch: `console: add pointer input and title bar controls`

This document describes the staged pointer-control layer for the VitaOS neon terminal shell.

## Goal

The neon terminal shell has a visual title bar with three controls:

```text
[MIN] [RESTART] [POWER]
```

This patch makes the UEFI input loop try to detect pointer clicks on those controls when the firmware exposes either:

- UEFI Simple Pointer Protocol
- UEFI Absolute Pointer Protocol

If no pointer protocol exists, the console continues working normally with the keyboard.

## Controls

| Control | Command emitted | Current behavior |
| --- | --- | --- |
| `[MIN]` | `minimize` | Uses the existing terminal minimize flow from the neon shell patch. |
| `[RESTART]` | `restart` | Staged only. It records/prints that restart was requested. Real safe reset comes later. |
| `[POWER]` | `shutdown` | Uses the existing shutdown command session flow. Real firmware poweroff comes later. |

## Why restart/power are staged

A real restart or shutdown must be safe for the audit-first design:

1. flush the text journal;
2. flush storage writes;
3. close editor/session state;
4. wait for AI/network operations to finish or mark them interrupted;
5. then call UEFI `ResetSystem()`.

That belongs in the next power-flow patch.

## Limitations

- The pointer coordinate mapping is conservative and text-mode oriented.
- Simple Pointer devices provide relative movement, so the firmware behavior may vary.
- Absolute Pointer devices map better when the firmware exposes min/max ranges.
- No graphical cursor is drawn yet.
- No framebuffer compositor or desktop is introduced.
- This is not a GUI/window manager.

## Keyboard fallback

Use the equivalent commands when pointer input is unavailable:

```text
minimize
restore
maximize
restart
shutdown
```
