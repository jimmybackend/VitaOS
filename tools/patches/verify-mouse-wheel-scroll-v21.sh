#!/usr/bin/env bash
set -euo pipefail

ROOT=$(pwd)
MAKEFILE="$ROOT/Makefile"
UEFI_ENTRY="$ROOT/arch/x86_64/boot/uefi_entry.c"
NEON_C="$ROOT/arch/x86_64/boot/uefi_neon_text.c"
NEON_H="$ROOT/arch/x86_64/boot/uefi_neon_text.h"
DOC="$ROOT/docs/console/mouse-wheel-scroll-indicator.md"

if [[ ! -f "$MAKEFILE" || ! -f "$UEFI_ENTRY" || ! -f "$NEON_C" || ! -f "$NEON_H" ]]; then
  echo "[v21-verify] ERROR: run from VitaOS repository root after applying v21 redo" >&2
  exit 1
fi

status=0

require_symbol() {
  local symbol="$1"
  shift
  if ! grep -q "$symbol" "$@" 2>/dev/null; then
    echo "[v21-verify] ERROR: missing symbol: $symbol" >&2
    status=1
  else
    echo "[v21-verify] OK: $symbol"
  fi
}

require_symbol 'arch/x86_64/boot/uefi_neon_text.c' "$MAKEFILE"
require_symbol 'uefi_neon_text.h' "$UEFI_ENTRY"
require_symbol 'EFI_SCAN_PAGE_UP' "$UEFI_ENTRY"
require_symbol 'EFI_SCAN_PAGE_DOWN' "$UEFI_ENTRY"
require_symbol 'g_simple_pointer_protocol_guid' "$UEFI_ENTRY"
require_symbol 'g_absolute_pointer_protocol_guid' "$UEFI_ENTRY"
require_symbol 'uefi_pointer_init' "$UEFI_ENTRY"
require_symbol 'uefi_poll_pointer_scroll' "$UEFI_ENTRY"
require_symbol 'vita_uefi_neon_text_init(image_handle, system_table);' "$UEFI_ENTRY"
require_symbol 'vita_uefi_neon_text_scroll_wheel' "$NEON_C" "$NEON_H"
require_symbol 'vita_uefi_neon_text_set_wheel_available' "$NEON_C" "$NEON_H"
require_symbol 'draw_scroll_indicator' "$NEON_C"
require_symbol 'wheel_available' "$NEON_C"

if [[ ! -f "$DOC" ]]; then
  echo "[v21-verify] ERROR: missing documentation: docs/console/mouse-wheel-scroll-indicator.md" >&2
  status=1
else
  echo "[v21-verify] OK: documentation present"
fi

if grep -RInE '(^#!.*python|python3|[^A-Za-z0-9_]python[^A-Za-z0-9_])' \
  "$MAKEFILE" "$UEFI_ENTRY" "$NEON_C" "$NEON_H" tools/patches 2>/dev/null; then
  echo "[v21-verify] ERROR: unexpected Python reference in v21 runtime/build files" >&2
  status=1
else
  echo "[v21-verify] OK: no Python dependency in v21 files"
fi

exit "$status"
