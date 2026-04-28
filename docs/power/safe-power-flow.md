# VitaOS safe power flow

This document describes the first F1A/F1B safe restart/shutdown flow.

## Purpose

The centered neon terminal has visual controls for:

```text
[MIN] [RESTART] [POWER]
```

This patch wires the restart/shutdown path through a common power module before the system exits or asks UEFI firmware to reset the machine.

## Commands

```text
power
power status
restart
reboot
shutdown
poweroff
exit
```

## Behavior

Before restart or shutdown, VitaOS attempts to:

1. record the power request in the audit/event path;
2. write a visible session journal event;
3. flush the session journal;
4. report storage readiness;
5. explain that remote AI jobs are still staged in this milestone;
6. in UEFI, call `ResetSystem()` when firmware exposes Runtime Services;
7. if shutdown cannot call ResetSystem, stop the command loop and let `kmain()` enter halt.

## UEFI behavior

- `restart` / `reboot` requests `EfiResetCold`.
- `shutdown` / `poweroff` requests `EfiResetShutdown`.
- If firmware returns unexpectedly or does not expose Runtime Services, VitaOS prints a clear diagnostic.

## Hosted behavior

- `shutdown`, `poweroff`, and `exit` leave the command loop.
- `restart` is reported as unsupported in hosted mode and the console remains active.

## Limits

This is not a full power manager yet.

Not included:

- ACPI shutdown fallback;
- real wait queue for remote AI jobs;
- filesystem-wide sync;
- editor transaction manager;
- encrypted journal;
- GUI/window-manager behavior.

The design remains text-first, audit-first, and freestanding-friendly.
