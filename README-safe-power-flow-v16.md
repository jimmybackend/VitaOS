# Patch v16 — power: add safe restart and shutdown flow

This patch adds the first common safe power flow for VitaOS F1A/F1B.

## Adds

```text
include/vita/power.h
kernel/power.c
docs/power/safe-power-flow.md
tools/patches/apply-safe-power-flow-v16.py
```

## Modifies through script

```text
Makefile
kernel/command_core.c
kernel/console.c
```

## New commands

```text
power
power status
restart
reboot
shutdown
poweroff
exit
```

## What it does

Before shutdown/restart, VitaOS now routes through `power_request()`:

- records the intent in audit/journal;
- flushes the visible session journal when active;
- prints storage/journal status;
- explains staged AI transport state;
- in UEFI, calls `ResetSystem()` when Runtime Services are available;
- falls back to halt for shutdown if UEFI poweroff does not complete;
- exits the hosted command loop for hosted shutdown.

## Not included

- ACPI shutdown fallback
- full filesystem sync
- waiting for real remote AI jobs
- real GUI window buttons
- framebuffer compositor
- malloc
- new runtime dependencies

## Apply

```bash
cd ~/VitaOS
unzip -o ~/vitaos-safe-power-flow-v16.zip -d .
python3 tools/patches/apply-safe-power-flow-v16.py
```

## Build

```bash
make clean && \
make hosted && \
make smoke-audit && \
make && \
make iso
```

## Test

```text
power
restart
shutdown
```

Hosted restart should stay active and explain that restart is unsupported. Hosted shutdown exits the command loop. UEFI restart/shutdown should attempt firmware `ResetSystem()`.
