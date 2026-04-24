#!/usr/bin/env bash
set -euo pipefail

# tools/image/make-uefi-usb-image.sh
#
# Build a minimal VitaOS x86_64 UEFI USB image for the current F1A/F1B slice.
#
# What this creates:
# - a raw disk image with GPT
# - one EFI System Partition (FAT32)
# - EFI/BOOT/BOOTX64.EFI copied from build/efi/EFI/BOOT/BOOTX64.EFI
# - /img/1.bmp ... /img/4.bmp copied as boot splash frames
#
# This script does NOT write to a physical USB device by itself.
# It only creates an image that can be tested in QEMU or written manually with dd.

IMAGE_PATH=${1:-build/image/vitaos-uefi-live.img}
IMAGE_SIZE_MIB=${IMAGE_SIZE_MIB:-128}
BUILD_EFI=${BUILD_EFI:-1}
EFI_BINARY=${EFI_BINARY:-build/efi/EFI/BOOT/BOOTX64.EFI}
SPLASH_ASSETS=(
  "img/1.bmp"
  "img/2.bmp"
  "img/3.bmp"
  "img/4.bmp"
)
PART_START_SECTOR=${PART_START_SECTOR:-2048}
SECTOR_SIZE=512
PART_OFFSET_BYTES=$((PART_START_SECTOR * SECTOR_SIZE))
MTOOLS_IMAGE="${IMAGE_PATH}@@${PART_OFFSET_BYTES}"

info() {
  printf '[vitaos-image] %s\n' "$*"
}

fail() {
  printf '[vitaos-image] ERROR: %s\n' "$*" >&2
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
  need_cmd dd
  need_cmd make
  need_cmd sgdisk
  need_cmd mkfs.fat
  need_cmd mmd
  need_cmd mcopy
  need_cmd mdir
  need_cmd truncate
}

check_mkfs_offset_support() {
  if ! mkfs.fat --help 2>&1 | grep -q -- '--offset'; then
    fail "mkfs.fat on this system does not support --offset. Install a recent dosfstools package."
  fi
}

check_splash_assets() {
  local asset

  for asset in "${SPLASH_ASSETS[@]}"; do
    if [[ ! -f "$asset" ]]; then
      fail "missing splash asset: $asset"
    fi
  done
}

build_efi_if_needed() {
  if [[ "$BUILD_EFI" == "0" ]]; then
    info "skipping EFI build because BUILD_EFI=0"
    return 0
  fi

  info "building VitaOS EFI binary with: make efi"
  make efi
}

create_empty_image() {
  local image_dir
  image_dir=$(dirname "$IMAGE_PATH")
  mkdir -p "$image_dir"

  rm -f "$IMAGE_PATH"

  info "creating raw disk image: $IMAGE_PATH (${IMAGE_SIZE_MIB} MiB)"
  truncate -s "${IMAGE_SIZE_MIB}M" "$IMAGE_PATH"
}

create_gpt_layout() {
  info "creating GPT with one EFI System Partition"

  sgdisk --zap-all "$IMAGE_PATH" >/dev/null
  sgdisk \
    --new=1:${PART_START_SECTOR}:0 \
    --typecode=1:EF00 \
    --change-name=1:VITAOS_EFI \
    "$IMAGE_PATH" >/dev/null

  info "partition 1 starts at sector ${PART_START_SECTOR} (${PART_OFFSET_BYTES} bytes)"
}

format_efi_partition() {
  info "formatting EFI System Partition as FAT32"
  mkfs.fat -F 32 -n VITAOS_EFI --offset "$PART_START_SECTOR" "$IMAGE_PATH" >/dev/null
}

copy_efi_files() {
  if [[ ! -f "$EFI_BINARY" ]]; then
    fail "EFI binary not found: $EFI_BINARY"
  fi

  info "copying EFI boot file"

  # mtools works directly with an image offset, so this does not need sudo/loop mounts.
  MTOOLS_SKIP_CHECK=1 mmd -i "$MTOOLS_IMAGE" ::/EFI || true
  MTOOLS_SKIP_CHECK=1 mmd -i "$MTOOLS_IMAGE" ::/EFI/BOOT || true
  MTOOLS_SKIP_CHECK=1 mcopy -o -i "$MTOOLS_IMAGE" "$EFI_BINARY" ::/EFI/BOOT/BOOTX64.EFI
}

copy_splash_assets() {
  local asset

  info "copying VitaOS boot splash frames"

  MTOOLS_SKIP_CHECK=1 mmd -i "$MTOOLS_IMAGE" ::/img || true

  for asset in "${SPLASH_ASSETS[@]}"; do
    MTOOLS_SKIP_CHECK=1 mcopy -o -i "$MTOOLS_IMAGE" "$asset" "::/$asset"
  done
}

verify_image() {
  local asset

  info "verifying EFI boot file and splash frames exist inside image"

  if ! MTOOLS_SKIP_CHECK=1 mdir -i "$MTOOLS_IMAGE" ::/EFI/BOOT/BOOTX64.EFI >/dev/null 2>&1; then
    fail "BOOTX64.EFI was not copied correctly into the image"
  fi

  for asset in "${SPLASH_ASSETS[@]}"; do
    if ! MTOOLS_SKIP_CHECK=1 mdir -i "$MTOOLS_IMAGE" "::/$asset" >/dev/null 2>&1; then
      fail "$asset was not copied correctly into the image"
    fi
  done
}

print_next_steps() {
  cat <<MSG

[vitaos-image] Image ready:
  $IMAGE_PATH

[vitaos-image] QEMU test example:
  qemu-system-x86_64 \\
    -machine q35,accel=tcg \\
    -cpu qemu64 \\
    -m 512M \\
    -display none \\
    -serial stdio \\
    -drive if=pflash,format=raw,readonly=on,file=/usr/share/OVMF/OVMF_CODE.fd \\
    -drive if=pflash,format=raw,file=/usr/share/OVMF/OVMF_VARS.fd \\
    -drive format=raw,file=$IMAGE_PATH

[vitaos-image] Physical USB write example, BE CAREFUL:
  lsblk
  sudo dd if=$IMAGE_PATH of=/dev/sdX bs=4M status=progress conv=fsync
  sync

Replace /dev/sdX with the real USB device, not a partition like /dev/sdX1.
MSG
}

main() {
  local root

  root=$(repo_root) || fail "run this script from inside the VitaOS repository"
  cd "$root"

  check_tools
  check_mkfs_offset_support
  check_splash_assets
  build_efi_if_needed
  create_empty_image
  create_gpt_layout
  format_efi_partition
  copy_efi_files
  copy_splash_assets
  verify_image
  print_next_steps
}

main "$@"
