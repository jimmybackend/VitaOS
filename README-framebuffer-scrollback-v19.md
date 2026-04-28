# VitaOS patch v19 — framebuffer scrollback viewport

Commit title:

```text
console: add framebuffer scrollback viewport
```

## What this changes

This patch extends the centered neon framebuffer terminal with a small in-memory scrollback viewport.

It replaces:

```text
arch/x86_64/boot/uefi_neon_text.c
arch/x86_64/boot/uefi_neon_text.h
```

and applies a small Bash/Perl patch to:

```text
arch/x86_64/boot/uefi_entry.c
```

## Why no Python

The previous `.py` files were only development-time patch helpers for the Linux build VM. They were never part of VitaOS and were never compiled into `BOOTX64.EFI`.

This patch switches the helper to a `.sh` script so the repo does not require Python to apply this specific change.

Do not add Python3 inside VitaOS for this milestone. A Python runtime would be a large userland feature and is outside F1A/F1B.

## Apply

From the repository root:

```bash
unzip -o ~/vitaos-framebuffer-scrollback-v19.zip -d .
bash tools/patches/apply-framebuffer-scrollback-v19.sh
```

## Build

```bash
make clean && \
make hosted && \
make smoke-audit && \
make && \
make iso
```

## Test in VitaOS UEFI

Run commands that produce long output:

```text
helpme
hw
emergency
journal show
export session
```

Then use:

```text
PageUp
PageDown
```

## Not included

- mouse wheel scroll;
- scrollbar graphic;
- ANSI terminal emulation;
- persistent framebuffer scrollback;
- GUI/window manager;
- malloc;
- new runtime dependencies.
