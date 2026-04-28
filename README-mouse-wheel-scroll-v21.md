# VitaOS patch v21 — mouse wheel and visual scroll indicator

Commit title:

```text
console: add mouse wheel and visual scroll indicator
```

## What this changes

This patch extends the current UEFI neon framebuffer scrollback with:

- PageUp/PageDown preserved;
- Simple Pointer Protocol `RelativeMovementZ` polling for wheel-style movement;
- Absolute Pointer Protocol `CurrentZ` delta polling where firmware exposes it;
- fallback-safe behavior when no pointer or no wheel is available;
- a small visual scroll indicator on the right side of the neon terminal body;
- a `wheel:on` / `wheel:n/a` status marker inside the panel;
- no malloc and no Python.

## Apply

Apply v20 first, then:

```bash
bash tools/patches/apply-mouse-wheel-scroll-v21.sh
```

## Validate

```bash
make clean
make hosted
make smoke-audit
make
make smoke
make iso
```

## Manual UEFI test

Run commands that produce long output:

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
mouse wheel up/down if firmware exposes wheel Z movement
clear
status
storage status
journal flush
shutdown/restart flow
```

## Scope boundaries

This is still text-first F1A/F1B. It does not add a GUI, desktop, window manager, real Wi-Fi, network expansion, malloc, or Python.
