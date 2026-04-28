#!/usr/bin/env bash
set -euo pipefail

# VitaOS patch v21 redo: mouse wheel scroll + visual scroll indicator.
# Bash/perl only. No Python, no malloc, no GUI/window manager.

ROOT=$(pwd)
MAKEFILE="$ROOT/Makefile"
UEFI_ENTRY="$ROOT/arch/x86_64/boot/uefi_entry.c"
NEON_C="$ROOT/arch/x86_64/boot/uefi_neon_text.c"
NEON_H="$ROOT/arch/x86_64/boot/uefi_neon_text.h"
DOC_DIR="$ROOT/docs/console"
DOC="$DOC_DIR/mouse-wheel-scroll-indicator.md"
VERIFY="$ROOT/tools/patches/verify-mouse-wheel-scroll-v21.sh"

if [[ ! -f "$MAKEFILE" || ! -f "$UEFI_ENTRY" || ! -f "$NEON_C" || ! -f "$NEON_H" ]]; then
  echo "[v21-redo] ERROR: run this from the VitaOS repository root with current UEFI neon files" >&2
  exit 1
fi

mkdir -p "$DOC_DIR" "$ROOT/tools/patches"

ts=$(date +%Y%m%d-%H%M%S)
cp -p "$MAKEFILE" "$MAKEFILE.v21-redo.$ts.bak"
cp -p "$UEFI_ENTRY" "$UEFI_ENTRY.v21-redo.$ts.bak"
cp -p "$NEON_C" "$NEON_C.v21-redo.$ts.bak"
cp -p "$NEON_H" "$NEON_H.v21-redo.$ts.bak"

if ! grep -q 'arch/x86_64/boot/uefi_neon_text.c' "$MAKEFILE"; then
  perl -0pi -e 's/(EFI_SOURCES := \\\n\tarch\/x86_64\/boot\/uefi_entry\.c \\\n\tarch\/x86_64\/boot\/uefi_splash\.c \\\n)/$1\tarch\/x86_64\/boot\/uefi_neon_text.c \\\n/' "$MAKEFILE"
fi

if ! grep -q 'vita_uefi_neon_text_scroll_wheel' "$NEON_H"; then
  perl -0pi -e 's/(void vita_uefi_neon_text_scroll_bottom\(void\);\n)/$1void vita_uefi_neon_text_set_wheel_available(int available);\nvoid vita_uefi_neon_text_scroll_wheel(int direction);\n/' "$NEON_H"
fi

if ! grep -q 'wheel_available' "$NEON_C"; then
  perl -0pi -e 's/(    uint32_t view_offset;\n)/$1    uint32_t wheel_available;\n/' "$NEON_C"
fi

if ! grep -q 'static void draw_scroll_indicator' "$NEON_C"; then
  perl -0pi -e 's/(static void render_body\(void\) \{)/static void draw_scroll_indicator(void) {\n    uint32_t rail_x;\n    uint32_t rail_y;\n    uint32_t rail_w;\n    uint32_t rail_h;\n    uint32_t thumb_h;\n    uint32_t thumb_y;\n    uint32_t max_offset;\n    uint32_t status_x;\n\n    if (!g_neon.ready || g_neon.body_w < 24U || g_neon.body_h < 48U) {\n        return;\n    }\n\n    rail_w = 4U;\n    rail_h = (g_neon.body_h > 36U) ? (g_neon.body_h - 36U) : g_neon.body_h;\n    rail_x = g_neon.body_x + g_neon.body_w - 10U;\n    rail_y = g_neon.body_y + 18U;\n\n    fill_rect(rail_x, rail_y, rail_w, rail_h, g_neon.dim);\n\n    max_offset = max_scroll_offset();\n    if (max_offset == 0U) {\n        thumb_h = rail_h;\n        thumb_y = rail_y;\n    } else {\n        thumb_h = rail_h \/ 4U;\n        if (thumb_h < 16U) {\n            thumb_h = 16U;\n        }\n        if (thumb_h > rail_h) {\n            thumb_h = rail_h;\n        }\n        thumb_y = rail_y + (((max_offset - g_neon.view_offset) * (rail_h - thumb_h)) \/ max_offset);\n    }\n\n    fill_rect(rail_x - 1U,\n              thumb_y,\n              rail_w + 2U,\n              thumb_h,\n              (g_neon.view_offset > 0U) ? g_neon.warning : g_neon.border);\n\n    if (g_neon.columns > 12U) {\n        status_x = g_neon.body_x + g_neon.body_w - (12U * g_neon.char_w);\n        draw_text_at(status_x,\n                     g_neon.body_y + 6U,\n                     g_neon.wheel_available ? "wheel:on" : "wheel:n\/a",\n                     g_neon.wheel_available ? g_neon.text : g_neon.dim,\n                     g_neon.panel_bg);\n    }\n}\n\n$1/' "$NEON_C"
fi

if ! grep -q 'draw_scroll_indicator();' "$NEON_C"; then
  perl -0pi -e 's/(    if \(g_neon\.view_offset > 0U\) \{\n)/    draw_scroll_indicator();\n\n$1/' "$NEON_C"
fi

if ! grep -q 'void vita_uefi_neon_text_set_wheel_available' "$NEON_C"; then
  cat >> "$NEON_C" <<'CADD'

void vita_uefi_neon_text_set_wheel_available(int available) {
    if (!g_neon.ready) {
        return;
    }

    g_neon.wheel_available = available ? 1U : 0U;
    render_body();
}

void vita_uefi_neon_text_scroll_wheel(int direction) {
    if (!g_neon.ready) {
        return;
    }

    g_neon.wheel_available = 1U;

    if (direction > 0) {
        vita_uefi_neon_text_scroll_up();
        return;
    }

    if (direction < 0) {
        vita_uefi_neon_text_scroll_down();
        return;
    }

    render_body();
}
CADD
fi

if ! grep -q 'uefi_neon_text.h' "$UEFI_ENTRY"; then
  perl -0pi -e 's/#include "uefi_splash.h"\n/#include "uefi_splash.h"\n#include "uefi_neon_text.h"\n/' "$UEFI_ENTRY"
fi

if ! grep -q 'EFI_SCAN_PAGE_UP' "$UEFI_ENTRY"; then
  perl -0pi -e 's/#define EFI_SCAN_DOWN 0x0002U\n/#define EFI_SCAN_DOWN 0x0002U\n#define EFI_SCAN_PAGE_UP 0x0009U\n#define EFI_SCAN_PAGE_DOWN 0x000aU\n/' "$UEFI_ENTRY"
fi

if ! grep -q 'efi_guid_t' "$UEFI_ENTRY"; then
  perl -0pi -e 's/(typedef struct \{\n    uint64_t signature;\n    uint32_t revision;\n    uint32_t header_size;\n    uint32_t crc32;\n    uint32_t reserved;\n\} efi_table_header_t;\n)/$1\ntypedef struct {\n    uint32_t data1;\n    uint16_t data2;\n    uint16_t data3;\n    uint8_t data4[8];\n} efi_guid_t;\n/' "$UEFI_ENTRY"
fi

if ! grep -q 'locate_protocol' "$UEFI_ENTRY"; then
  perl -0pi -e 's/(    efi_status_t \(\*stall\)\(uint64_t microseconds\);\n)/$1    void *set_watchdog_timer;\n    void *connect_controller;\n    void *disconnect_controller;\n    void *open_protocol;\n    void *close_protocol;\n    void *open_protocol_information;\n    void *protocols_per_handle;\n    void *locate_handle_buffer;\n    efi_status_t (*locate_protocol)(efi_guid_t *protocol, void *registration, void **interface);\n    void *install_multiple_protocol_interfaces;\n    void *uninstall_multiple_protocol_interfaces;\n    void *calculate_crc32;\n    void *copy_mem;\n    void *set_mem;\n    void *create_event_ex;\n/' "$UEFI_ENTRY"
fi

if ! grep -q 'VITA_MOUSE_SCROLL_TYPES_START' "$UEFI_ENTRY"; then
  perl -0pi -e 's/(void kmain\(const vita_handoff_t \*handoff\);\n)/\/\* VITA_MOUSE_SCROLL_TYPES_START \*\/\ntypedef struct efi_simple_pointer_protocol efi_simple_pointer_protocol_t;\ntypedef struct efi_absolute_pointer_protocol efi_absolute_pointer_protocol_t;\n\ntypedef struct {\n    int32_t relative_movement_x;\n    int32_t relative_movement_y;\n    int32_t relative_movement_z;\n    uint8_t left_button;\n    uint8_t right_button;\n} efi_simple_pointer_state_t;\n\ntypedef struct {\n    uint64_t resolution_x;\n    uint64_t resolution_y;\n    uint64_t resolution_z;\n    uint8_t left_button;\n    uint8_t right_button;\n} efi_simple_pointer_mode_t;\n\nstruct efi_simple_pointer_protocol {\n    efi_status_t (*reset)(efi_simple_pointer_protocol_t *self, uint8_t extended_verification);\n    efi_status_t (*get_state)(efi_simple_pointer_protocol_t *self, efi_simple_pointer_state_t *state);\n    efi_event_t wait_for_input;\n    efi_simple_pointer_mode_t *mode;\n};\n\ntypedef struct {\n    uint64_t current_x;\n    uint64_t current_y;\n    uint64_t current_z;\n    uint32_t active_buttons;\n} efi_absolute_pointer_state_t;\n\ntypedef struct {\n    uint64_t absolute_min_x;\n    uint64_t absolute_min_y;\n    uint64_t absolute_min_z;\n    uint64_t absolute_max_x;\n    uint64_t absolute_max_y;\n    uint64_t absolute_max_z;\n    uint32_t attributes;\n} efi_absolute_pointer_mode_t;\n\nstruct efi_absolute_pointer_protocol {\n    efi_status_t (*reset)(efi_absolute_pointer_protocol_t *self, uint8_t extended_verification);\n    efi_status_t (*get_state)(efi_absolute_pointer_protocol_t *self, efi_absolute_pointer_state_t *state);\n    efi_event_t wait_for_input;\n    efi_absolute_pointer_mode_t *mode;\n};\n\nstatic efi_guid_t g_simple_pointer_protocol_guid = {\n    0x31878c87, 0x0b75, 0x11d5,\n    {0x9a, 0x4f, 0x00, 0x90, 0x27, 0x3f, 0xc1, 0x4d}\n};\n\nstatic efi_guid_t g_absolute_pointer_protocol_guid = {\n    0x8d59d32b, 0xc655, 0x4ae9,\n    {0x9b, 0x15, 0xf2, 0x59, 0x04, 0x99, 0x2a, 0x43}\n};\n\nstatic efi_simple_pointer_protocol_t *g_simple_pointer = 0;\nstatic efi_absolute_pointer_protocol_t *g_absolute_pointer = 0;\nstatic uint64_t g_absolute_last_z = 0;\nstatic int g_absolute_has_last_z = 0;\n\/\* VITA_MOUSE_SCROLL_TYPES_END \*\/\n\n$1/' "$UEFI_ENTRY"
fi

if ! grep -q 'vita_uefi_neon_text_write_raw(text);' "$UEFI_ENTRY"; then
  perl -0pi -e 's/(static void uefi_output_raw\(const char \*text\) \{\n    char16_t buffer\[256\];\n)/$1\n    if (vita_uefi_neon_text_ready()) {\n        vita_uefi_neon_text_write_raw(text);\n        return;\n    }\n/' "$UEFI_ENTRY"
fi

if ! grep -q 'vita_uefi_neon_text_write_line(text);' "$UEFI_ENTRY"; then
  perl -0pi -e 's/(static void uefi_console_write\(const char \*text\) \{\n    char16_t buffer\[256\];\n)/$1\n    if (vita_uefi_neon_text_ready()) {\n        vita_uefi_neon_text_write_line(text);\n        return;\n    }\n/' "$UEFI_ENTRY"
fi

if ! grep -q 'vita_uefi_neon_text_clear();' "$UEFI_ENTRY"; then
  perl -0pi -e 's/(static void uefi_console_clear\(void\) \{\n)/$1    if (vita_uefi_neon_text_ready()) {\n        vita_uefi_neon_text_clear();\n        return;\n    }\n\n/' "$UEFI_ENTRY"
fi

if ! grep -q 'static void uefi_pointer_init' "$UEFI_ENTRY"; then
  perl -0pi -e 's/(static void uefi_poll_delay\(void\) \{)/static void uefi_pointer_init(void) {\n    void *iface = 0;\n\n    g_simple_pointer = 0;\n    g_absolute_pointer = 0;\n    g_absolute_last_z = 0;\n    g_absolute_has_last_z = 0;\n\n    if (!g_st || !g_st->boot_services || !g_st->boot_services->locate_protocol) {\n        vita_uefi_neon_text_set_wheel_available(0);\n        return;\n    }\n\n    if (g_st->boot_services->locate_protocol(&g_simple_pointer_protocol_guid, 0, &iface) == EFI_SUCCESS && iface) {\n        g_simple_pointer = (efi_simple_pointer_protocol_t *)iface;\n        if (g_simple_pointer->reset) {\n            (void)g_simple_pointer->reset(g_simple_pointer, 0);\n        }\n    }\n\n    iface = 0;\n    if (g_st->boot_services->locate_protocol(&g_absolute_pointer_protocol_guid, 0, &iface) == EFI_SUCCESS && iface) {\n        g_absolute_pointer = (efi_absolute_pointer_protocol_t *)iface;\n        if (g_absolute_pointer->reset) {\n            (void)g_absolute_pointer->reset(g_absolute_pointer, 0);\n        }\n    }\n\n    vita_uefi_neon_text_set_wheel_available(0);\n}\n\nstatic void uefi_poll_pointer_scroll(void) {\n    efi_simple_pointer_state_t simple_state;\n    efi_absolute_pointer_state_t abs_state;\n\n    if (g_simple_pointer && g_simple_pointer->get_state) {\n        if (g_simple_pointer->get_state(g_simple_pointer, &simple_state) == EFI_SUCCESS) {\n            if (simple_state.relative_movement_z > 0) {\n                vita_uefi_neon_text_scroll_wheel(1);\n            } else if (simple_state.relative_movement_z < 0) {\n                vita_uefi_neon_text_scroll_wheel(-1);\n            }\n        }\n    }\n\n    if (g_absolute_pointer && g_absolute_pointer->get_state) {\n        if (g_absolute_pointer->get_state(g_absolute_pointer, &abs_state) == EFI_SUCCESS) {\n            if (!g_absolute_has_last_z) {\n                g_absolute_last_z = abs_state.current_z;\n                g_absolute_has_last_z = 1;\n            } else if (abs_state.current_z > g_absolute_last_z) {\n                g_absolute_last_z = abs_state.current_z;\n                vita_uefi_neon_text_scroll_wheel(1);\n            } else if (abs_state.current_z < g_absolute_last_z) {\n                g_absolute_last_z = abs_state.current_z;\n                vita_uefi_neon_text_scroll_wheel(-1);\n            }\n        }\n    }\n}\n\n$1/' "$UEFI_ENTRY"
fi

if ! grep -q 'uefi_poll_pointer_scroll();' "$UEFI_ENTRY"; then
  perl -0pi -e 's/(        if \(!uefi_try_read_key\(&key\)\) \{\n)/$1            uefi_poll_pointer_scroll();\n/' "$UEFI_ENTRY"
fi

if ! grep -q 'key.scan_code == EFI_SCAN_PAGE_UP' "$UEFI_ENTRY"; then
  perl -0pi -e 's/\n        if \(key\.unicode_char == 13 \|\| key\.unicode_char == 10\) \{/\n        if (key.unicode_char == 0 && key.scan_code == EFI_SCAN_PAGE_UP) {\n            vita_uefi_neon_text_scroll_up();\n            continue;\n        }\n\n        if (key.unicode_char == 0 && key.scan_code == EFI_SCAN_PAGE_DOWN) {\n            vita_uefi_neon_text_scroll_down();\n            continue;\n        }\n\n        if (key.unicode_char == 13 || key.unicode_char == 10) {/' "$UEFI_ENTRY"
fi

if ! grep -q 'vita_uefi_neon_text_init(image_handle, system_table);' "$UEFI_ENTRY"; then
  perl -0pi -e 's/(    vita_uefi_show_splash\(image_handle, system_table\);\n)/$1\n    vita_uefi_neon_text_init(image_handle, system_table);\n    uefi_pointer_init();\n/' "$UEFI_ENTRY"
fi

cat > "$DOC" <<'DOC'
# Mouse wheel scroll and visual indicator

Patch: `console: add mouse wheel and visual scroll indicator`

## Behavior

The UEFI neon framebuffer terminal keeps keyboard scrollback and adds optional pointer wheel scrolling.

Supported input paths:

- `PageUp`: scrolls up through the in-memory framebuffer scrollback.
- `PageDown`: scrolls down toward live output.
- Simple Pointer Protocol: uses `RelativeMovementZ` when firmware reports it.
- Absolute Pointer Protocol: uses `CurrentZ` deltas when firmware reports compatible movement.

If no pointer protocol is present, or if the pointer does not expose wheel-compatible Z movement, VitaOS continues normally and shows `wheel:n/a` inside the panel.

## Scope

This is not a GUI and not a window manager. It does not add mouse cursor drawing, widgets, desktop behavior, networking, Wi-Fi, malloc, Python, or new OS dependencies.

The scrollback remains RAM-only. Persistent command/event history remains the visible session journal under `/vita/audit/`.
DOC

if grep -RInE '(^#!.*python|python3|[^A-Za-z0-9_]python[^A-Za-z0-9_])' \
  "$MAKEFILE" "$UEFI_ENTRY" "$NEON_C" "$NEON_H" 2>/dev/null; then
  echo "[v21-redo] ERROR: unexpected Python reference in patched runtime/build files" >&2
  exit 1
fi

bash "$VERIFY"

echo "[v21-redo] mouse wheel scroll and visual scroll indicator applied"
echo "[v21-redo] next: make clean && make hosted && make smoke-audit && make && make smoke && make iso"
