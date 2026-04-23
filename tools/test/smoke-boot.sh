#!/usr/bin/env bash
set -euo pipefail

QEMU_BIN=${QEMU_BIN:-qemu-system-x86_64}
OVMF_CODE=${OVMF_CODE:-/usr/share/OVMF/OVMF_CODE.fd}
OVMF_VARS=${OVMF_VARS:-/usr/share/OVMF/OVMF_VARS.fd}

LOG_DIR=build/test
LOG_FILE="$LOG_DIR/smoke-boot.log"

mkdir -p "$LOG_DIR"
rm -f "$LOG_FILE"

if ! command -v "$QEMU_BIN" >/dev/null 2>&1; then
  echo "SKIP: qemu not installed" >&2
  exit 0
fi

if [[ ! -f "$OVMF_CODE" || ! -f "$OVMF_VARS" ]]; then
  echo "SKIP: OVMF files missing" >&2
  exit 0
fi

if [[ ! -f build/efi/EFI/BOOT/BOOTX64.EFI ]]; then
  echo "missing EFI binary" >&2
  exit 1
fi

set +e
"$QEMU_BIN" \
  -machine q35,accel=tcg \
  -cpu qemu64 \
  -m 512M \
  -serial stdio \
  -display none \
  -no-reboot \
  -drive if=pflash,format=raw,readonly=on,file="$OVMF_CODE" \
  -drive if=pflash,format=raw,file="$OVMF_VARS" \
  -drive format=raw,file=fat:rw:build/efi \
  -monitor none \
  >"$LOG_FILE" 2>&1 &
PID=$!

sleep 5
kill "$PID" >/dev/null 2>&1 || true
wait "$PID" >/dev/null 2>&1 || true
set -e

if ! grep -q "VitaOS with AI / VitaOS con IA" "$LOG_FILE"; then
  echo "smoke failed: banner not found" >&2
  cat "$LOG_FILE" >&2
  exit 1
fi

if ! grep -q "Boot: OK" "$LOG_FILE"; then
  echo "smoke failed: Boot status not found" >&2
  exit 1
fi

if ! grep -q "Arch: x86_64" "$LOG_FILE"; then
  echo "smoke failed: arch line not found" >&2
  exit 1
fi

if ! grep -q "Console: OK" "$LOG_FILE"; then
  echo "smoke failed: console line not found" >&2
  exit 1
fi

if ! grep -q "Audit: FAILED" "$LOG_FILE"; then
  echo "smoke failed: expected restricted EFI audit state" >&2
  exit 1
fi

if ! grep -q "Operational mode blocked / Modo operativo bloqueado" "$LOG_FILE"; then
  echo "smoke failed: restricted guided mode line not found" >&2
  exit 1
fi

echo "smoke ok"
