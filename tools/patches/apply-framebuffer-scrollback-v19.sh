#!/usr/bin/env bash
set -euo pipefail

# Apply VitaOS patch v19: framebuffer scrollback viewport.
# This script intentionally uses bash/perl only. No Python is required.

ROOT=$(pwd)
UEFI_ENTRY="$ROOT/arch/x86_64/boot/uefi_entry.c"

if [[ ! -f "$UEFI_ENTRY" ]]; then
  echo "[v19] ERROR: run this from the VitaOS repository root" >&2
  exit 1
fi

if ! grep -q 'EFI_SCAN_PAGE_UP' "$UEFI_ENTRY"; then
  perl -0pi -e 's/#define EFI_SCAN_DOWN 0x0002U\n/#define EFI_SCAN_DOWN 0x0002U\n#define EFI_SCAN_PAGE_UP 0x0009U\n#define EFI_SCAN_PAGE_DOWN 0x000aU\n/' "$UEFI_ENTRY"
fi

if ! grep -q 'vita_uefi_neon_text_scroll_up' "$UEFI_ENTRY"; then
  perl -0pi -e 's/\n        if \(key\.unicode_char == 13 \|\| key\.unicode_char == 10\) \{/\n        if (key.unicode_char == 0 && key.scan_code == EFI_SCAN_PAGE_UP) {\n            if (vita_uefi_neon_text_ready()) {\n                vita_uefi_neon_text_scroll_up();\n            }\n            continue;\n        }\n\n        if (key.unicode_char == 0 && key.scan_code == EFI_SCAN_PAGE_DOWN) {\n            if (vita_uefi_neon_text_ready()) {\n                vita_uefi_neon_text_scroll_down();\n            }\n            continue;\n        }\n\n        if (key.unicode_char == 13 || key.unicode_char == 10) {/' "$UEFI_ENTRY"
fi

if ! grep -q 'vita_uefi_neon_text_scroll_up' arch/x86_64/boot/uefi_neon_text.h; then
  echo "[v19] ERROR: uefi_neon_text.h was not overwritten correctly. Extract ZIP with unzip -o first." >&2
  exit 1
fi

echo "[v19] framebuffer scrollback viewport patch applied"
