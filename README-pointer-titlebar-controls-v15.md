# Patch v15 — console: add pointer input and title bar controls

This patch adds a conservative UEFI pointer-control layer for the staged neon terminal shell.

## Files changed by the patch script

- `arch/x86_64/boot/uefi_entry.c`
- `kernel/console.c`
- `kernel/command_core.c`

## Files added

- `docs/console/pointer-titlebar-controls.md`
- `tools/patches/apply-pointer-titlebar-controls-v15.py`

## Apply

```bash
cd ~/VitaOS
unzip -o ~/vitaos-pointer-titlebar-controls-v15.zip -d .
python3 tools/patches/apply-pointer-titlebar-controls-v15.py
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

In VitaOS:

```text
minimize
restore
maximize
restart
shutdown
```

On UEFI hardware with a pointer protocol exposed by firmware, clicks near the title bar controls may emit:

```text
minimize
restart
shutdown
```

## Scope

This is pointer/control plumbing only.

Not included:

- real UEFI `ResetSystem()` restart/poweroff;
- journal/storage/editor flush orchestration;
- drawn mouse cursor;
- framebuffer terminal renderer;
- desktop/window manager;
- malloc;
- new runtime dependencies.

The safe power flow should come in the next patch.
