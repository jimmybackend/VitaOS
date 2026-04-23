#!/usr/bin/env bash
set -euo pipefail

QEMU_BIN=${QEMU_BIN:-qemu-system-x86_64}
OVMF_CODE=${OVMF_CODE:-/usr/share/OVMF/OVMF_CODE.fd}
OVMF_VARS=${OVMF_VARS:-/usr/share/OVMF/OVMF_VARS.fd}

if ! command -v "$QEMU_BIN" >/dev/null 2>&1; then
  echo "qemu not found: $QEMU_BIN" >&2
  exit 1
fi

if [[ ! -f "$OVMF_CODE" || ! -f "$OVMF_VARS" ]]; then
  echo "OVMF files not found. Set OVMF_CODE/OVMF_VARS env vars." >&2
  exit 1
fi

if [[ ! -f build/efi/EFI/BOOT/BOOTX64.EFI ]]; then
  echo "Missing EFI binary. Run make first." >&2
  exit 1
fi

"$QEMU_BIN" \
  -machine q35,accel=tcg \
  -cpu qemu64 \
  -m 512M \
  -serial stdio \
  -display none \
  -drive if=pflash,format=raw,readonly=on,file="$OVMF_CODE" \
  -drive if=pflash,format=raw,file="$OVMF_VARS" \
  -drive format=raw,file=fat:rw:build/efi
