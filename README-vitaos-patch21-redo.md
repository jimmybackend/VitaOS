# VitaOS patch v21 redo — mouse wheel and visual scroll indicator

Commit title:

```text
console: add mouse wheel and visual scroll indicator
```

## Why this redo exists

The previous v21 package may have landed as README/scripts only, without the source files being patched in the working tree. This redo patch applies the actual source changes idempotently.

## What it changes

- Adds the UEFI neon framebuffer text renderer to the EFI build path.
- Routes UEFI console output through the neon framebuffer renderer when GOP is ready.
- Preserves Simple Text Output fallback when GOP is not available.
- Preserves keyboard commands and history.
- Adds PageUp/PageDown scrollback keys if missing.
- Adds optional Simple Pointer Protocol wheel support through `RelativeMovementZ`.
- Adds optional Absolute Pointer Protocol Z-delta support through `CurrentZ`.
- Adds a right-side visual scroll indicator inside the neon panel.
- Adds `wheel:on` / `wheel:n/a` status inside the panel.
- Adds a Bash verification helper.

## Scope boundaries

No Python, no malloc, no GUI/window manager, no desktop, no Wi-Fi/network expansion, no new runtime dependency.

## Apply

From the VitaOS repo root:

```bash
unzip -o vitaos-patch21-mouse-wheel-scroll-redo.zip -d .
bash tools/patches/apply-mouse-wheel-scroll-v21-redo.sh
```

## Validate

```bash
bash tools/patches/verify-mouse-wheel-scroll-v21.sh
make clean
make hosted
make smoke-audit
make
make smoke
make iso
```

## Manual UEFI test

Run output-heavy commands:

```text
helpme
hw
journal show
export session
```

Then test:

```text
PageUp
PageDown
mouse wheel up/down if firmware exposes Z wheel movement
clear
status
storage status
journal flush
restart
shutdown
```

If no firmware wheel support is exposed, the panel should show `wheel:n/a` and keyboard scrollback must keep working.
