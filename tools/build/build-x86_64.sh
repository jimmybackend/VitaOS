#!/usr/bin/env bash
set -euo pipefail

make clean
make all

echo "Built EFI app: build/efi/EFI/BOOT/BOOTX64.EFI"
