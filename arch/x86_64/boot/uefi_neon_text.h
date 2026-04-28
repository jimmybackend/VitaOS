#ifndef VITA_UEFI_NEON_TEXT_H
#define VITA_UEFI_NEON_TEXT_H

/*
 * GOP-backed bitmap text renderer for the VitaOS neon terminal shell.
 *
 * This is intentionally UEFI-only and freestanding-friendly. It provides
 * a small fallback-safe text drawing layer for the existing guided console.
 */

void vita_uefi_neon_text_init(void *image_handle, void *system_table);
int vita_uefi_neon_text_ready(void);
void vita_uefi_neon_text_write_raw(const char *text);
void vita_uefi_neon_text_write_line(const char *text);
void vita_uefi_neon_text_clear(void);
void vita_uefi_neon_text_scroll_up(void);
void vita_uefi_neon_text_scroll_down(void);
void vita_uefi_neon_text_scroll_bottom(void);

#endif
