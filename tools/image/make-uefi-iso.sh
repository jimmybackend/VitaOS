#!/usr/bin/env bash
set -euo pipefail

# tools/image/make-uefi-iso.sh
#
# Build a minimal VitaOS x86_64 UEFI bootable ISO for the current F1A/F1B slice.
#
# What this creates:
# - build/iso/vitaos-uefi-live.iso by default
# - an El Torito UEFI FAT boot image inside the ISO
# - EFI/BOOT/BOOTX64.EFI copied into that FAT boot image
# - img/1.bmp ... img/4.bmp copied into that FAT boot image for the UEFI splash
#
# This ISO is intended for QEMU/VM testing and for firmware that supports UEFI CD/DVD ISO boot.
# It does not seed a full writable /vita tree in a USB-style FAT data area.
# For physical USB boot/storage validation, use tools/image/make-uefi-usb-image.sh instead.

ISO_PATH=${1:-build/iso/vitaos-uefi-live.iso}
BUILD_EFI=${BUILD_EFI:-1}
EFI_BINARY=${EFI_BINARY:-build/efi/EFI/BOOT/BOOTX64.EFI}
EFI_SPLASH_DIR=${EFI_SPLASH_DIR:-build/efi/img}
ISO_WORK_DIR=${ISO_WORK_DIR:-build/iso/work}
ISO_ROOT=${ISO_ROOT:-build/iso/root}
EFI_BOOT_IMAGE=${EFI_BOOT_IMAGE:-build/iso/root/EFI/BOOT/efiboot.img}
EFI_BOOT_IMAGE_SIZE_MIB=${EFI_BOOT_IMAGE_SIZE_MIB:-64}
ISO_VOLUME_ID=${ISO_VOLUME_ID:-VITAOS_EFI}

info() {
  printf '[vitaos-iso] %s\n' "$*"
}

fail() {
  printf '[vitaos-iso] ERROR: %s\n' "$*" >&2
  exit 1
}

need_cmd() {
  if ! command -v "$1" >/dev/null 2>&1; then
    fail "missing required command: $1"
  fi
}

repo_root() {
  local dir
  dir=$(pwd)

  while [[ "$dir" != "/" ]]; do
    if [[ -f "$dir/Makefile" && -d "$dir/kernel" && -d "$dir/arch" ]]; then
      printf '%s\n' "$dir"
      return 0
    fi
    dir=$(dirname "$dir")
  done

  return 1
}

check_tools() {
  need_cmd make
  need_cmd xorriso
  need_cmd mkfs.fat
  need_cmd mmd
  need_cmd mcopy
  need_cmd mdir
  need_cmd truncate
  need_cmd cp
  need_cmd rm
  need_cmd mkdir
}

build_efi_if_needed() {
  if [[ "$BUILD_EFI" == "0" ]]; then
    info "skipping EFI build because BUILD_EFI=0"
    return 0
  fi

  info "building VitaOS EFI binary with: make efi"
  make efi
}

check_inputs() {
  if [[ ! -f "$EFI_BINARY" ]]; then
    fail "EFI binary not found: $EFI_BINARY"
  fi

  for frame in 1 2 3 4; do
    if [[ ! -f "$EFI_SPLASH_DIR/${frame}.bmp" ]]; then
      fail "splash frame not found: $EFI_SPLASH_DIR/${frame}.bmp. Run make efi first."
    fi
  done
}

prepare_iso_root() {
  info "preparing ISO root: $ISO_ROOT"

  rm -rf "$ISO_WORK_DIR" "$ISO_ROOT"
  mkdir -p "$ISO_WORK_DIR" "$ISO_ROOT/EFI/BOOT" "$ISO_ROOT/img" "$(dirname "$ISO_PATH")"

  cp "$EFI_BINARY" "$ISO_ROOT/EFI/BOOT/BOOTX64.EFI"
  cp "$EFI_SPLASH_DIR"/*.bmp "$ISO_ROOT/img/"
}

create_efi_boot_image() {
  info "creating El Torito UEFI FAT boot image: $EFI_BOOT_IMAGE (${EFI_BOOT_IMAGE_SIZE_MIB} MiB)"

  rm -f "$EFI_BOOT_IMAGE"
  truncate -s "${EFI_BOOT_IMAGE_SIZE_MIB}M" "$EFI_BOOT_IMAGE"
  mkfs.fat -F 32 -n VITAOSISO "$EFI_BOOT_IMAGE" >/dev/null

  MTOOLS_SKIP_CHECK=1 mmd -i "$EFI_BOOT_IMAGE" ::/EFI || true
  MTOOLS_SKIP_CHECK=1 mmd -i "$EFI_BOOT_IMAGE" ::/EFI/BOOT || true
  MTOOLS_SKIP_CHECK=1 mmd -i "$EFI_BOOT_IMAGE" ::/img || true

  MTOOLS_SKIP_CHECK=1 mcopy -o -i "$EFI_BOOT_IMAGE" "$EFI_BINARY" ::/EFI/BOOT/BOOTX64.EFI
  for frame in 1 2 3 4; do
    MTOOLS_SKIP_CHECK=1 mcopy -o -i "$EFI_BOOT_IMAGE" "$EFI_SPLASH_DIR/${frame}.bmp" "::/img/${frame}.bmp"
  done
}

verify_efi_boot_image() {
  info "verifying files inside El Torito UEFI image"

  if ! MTOOLS_SKIP_CHECK=1 mdir -i "$EFI_BOOT_IMAGE" ::/EFI/BOOT/BOOTX64.EFI >/dev/null 2>&1; then
    fail "BOOTX64.EFI was not copied correctly into the EFI boot image"
  fi

  for frame in 1 2 3 4; do
    if ! MTOOLS_SKIP_CHECK=1 mdir -i "$EFI_BOOT_IMAGE" "::/img/${frame}.bmp" >/dev/null 2>&1; then
      fail "splash frame ${frame}.bmp was not copied correctly into the EFI boot image"
    fi
  done
}

create_iso() {
  info "creating UEFI bootable ISO: $ISO_PATH"

  rm -f "$ISO_PATH"

  xorriso -as mkisofs \
    -iso-level 3 \
    -R \
    -J \
    -joliet-long \
    -V "$ISO_VOLUME_ID" \
    -o "$ISO_PATH" \
    -e EFI/BOOT/efiboot.img \
    -no-emul-boot \
    -isohybrid-gpt-basdat \
    "$ISO_ROOT" >/dev/null
}

print_next_steps() {
  cat <<MSG

[vitaos-iso] ISO ready:
  $ISO_PATH

[vitaos-iso] QEMU ISO test example:
  qemu-system-x86_64 \\
    -machine q35,accel=tcg \\
    -cpu qemu64 \\
    -m 512M \\
    -display none \\
    -serial stdio \\
    -drive if=pflash,format=raw,readonly=on,file=/usr/share/OVMF/OVMF_CODE.fd \\
    -drive if=pflash,format=raw,file=/usr/share/OVMF/OVMF_VARS.fd \\
    -cdrom $ISO_PATH

[vitaos-iso] Note:
  This ISO is for UEFI ISO/CD boot testing only.
  It does not guarantee a pre-seeded writable /vita tree on physical USB media.
  For writing to a physical USB, prefer:
    ./tools/image/make-uefi-usb-image.sh
  Or run 'storage repair' after boot in a writable FAT environment (for example Rufus media).
MSG
}

main() {
  local root

  root=$(repo_root) || fail "run this script from inside the VitaOS repository"
  cd "$root"

  check_tools
  build_efi_if_needed
  check_inputs
  prepare_iso_root
  create_efi_boot_image
  verify_efi_boot_image
  create_iso
  print_next_steps
}

main "$@"
