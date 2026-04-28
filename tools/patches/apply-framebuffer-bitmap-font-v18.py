#!/usr/bin/env python3
"""
Apply VitaOS patch v18: console framebuffer bitmap font renderer.

This script is intentionally idempotent. It assumes the ZIP was extracted at
repo root and then patches the current local repository state.
"""
from pathlib import Path

ROOT = Path.cwd()


def read(path: str) -> str:
    return (ROOT / path).read_text()


def write(path: str, text: str) -> None:
    (ROOT / path).write_text(text)


def ensure_contains(path: str, needle: str, insert_after: str, block: str) -> None:
    text = read(path)
    if needle in text:
        return
    if insert_after not in text:
        raise SystemExit(f"Could not patch {path}: anchor not found: {insert_after!r}")
    text = text.replace(insert_after, insert_after + block, 1)
    write(path, text)


def patch_makefile() -> None:
    path = "Makefile"
    text = read(path)
    source = "\tarch/x86_64/boot/uefi_neon_text.c \\\n"
    if "arch/x86_64/boot/uefi_neon_text.c" in text:
        return

    if "\tarch/x86_64/boot/uefi_neon_frame.c \\\n" in text:
        text = text.replace("\tarch/x86_64/boot/uefi_neon_frame.c \\\n",
                            "\tarch/x86_64/boot/uefi_neon_frame.c \\\n" + source,
                            1)
    elif "\tarch/x86_64/boot/uefi_splash.c \\\n" in text:
        text = text.replace("\tarch/x86_64/boot/uefi_splash.c \\\n",
                            "\tarch/x86_64/boot/uefi_splash.c \\\n" + source,
                            1)
    else:
        raise SystemExit("Could not patch Makefile: EFI_SOURCES anchor not found")

    write(path, text)


def patch_uefi_entry() -> None:
    path = "arch/x86_64/boot/uefi_entry.c"
    text = read(path)

    if '#include "uefi_neon_text.h"' not in text:
        if '#include "uefi_neon_frame.h"' in text:
            text = text.replace('#include "uefi_neon_frame.h"\n',
                                '#include "uefi_neon_frame.h"\n#include "uefi_neon_text.h"\n',
                                1)
        elif '#include "uefi_splash.h"' in text:
            text = text.replace('#include "uefi_splash.h"\n',
                                '#include "uefi_splash.h"\n#include "uefi_neon_text.h"\n',
                                1)
        else:
            raise SystemExit("Could not patch uefi_entry.c: include anchor not found")

    raw_anchor = "static void uefi_output_raw(const char *text) {\n"
    raw_block = (
        "    if (vita_uefi_neon_text_ready()) {\n"
        "        vita_uefi_neon_text_write_raw(text);\n"
        "        return;\n"
        "    }\n\n"
    )
    if "vita_uefi_neon_text_write_raw(text);" not in text:
        if raw_anchor not in text:
            raise SystemExit("Could not patch uefi_entry.c: uefi_output_raw anchor not found")
        text = text.replace(raw_anchor, raw_anchor + raw_block, 1)

    write_anchor = "static void uefi_console_write(const char *text) {\n"
    write_block = (
        "    if (vita_uefi_neon_text_ready()) {\n"
        "        vita_uefi_neon_text_write_line(text);\n"
        "        return;\n"
        "    }\n\n"
    )
    if "vita_uefi_neon_text_write_line(text);" not in text:
        if write_anchor not in text:
            raise SystemExit("Could not patch uefi_entry.c: uefi_console_write anchor not found")
        text = text.replace(write_anchor, write_anchor + write_block, 1)

    clear_anchor = "static void uefi_console_clear(void) {\n"
    clear_block = (
        "    if (vita_uefi_neon_text_ready()) {\n"
        "        vita_uefi_neon_text_clear();\n"
        "        return;\n"
        "    }\n\n"
    )
    if "vita_uefi_neon_text_clear();" not in text:
        if clear_anchor not in text:
            raise SystemExit("Could not patch uefi_entry.c: uefi_console_clear anchor not found")
        text = text.replace(clear_anchor, clear_anchor + clear_block, 1)

    init_call = "    vita_uefi_neon_text_init(image_handle, system_table);\n"
    if "vita_uefi_neon_text_init(image_handle, system_table);" not in text:
        # Prefer to initialize after splash and before the existing clear.
        splash_call = "    vita_uefi_show_splash(image_handle, system_table);\n"
        if splash_call in text:
            text = text.replace(splash_call, splash_call + "\n" + init_call, 1)
        else:
            clear_call = "    uefi_console_clear();\n"
            if clear_call not in text:
                raise SystemExit("Could not patch uefi_entry.c: init anchor not found")
            text = text.replace(clear_call, init_call + clear_call, 1)

    write(path, text)


def main() -> None:
    patch_makefile()
    patch_uefi_entry()
    print("[v18] framebuffer bitmap font renderer patch applied")


if __name__ == "__main__":
    main()
