#!/usr/bin/env python3
from pathlib import Path
import re

ROOT = Path.cwd()


def read(path):
    return (ROOT / path).read_text()


def write(path, text):
    (ROOT / path).write_text(text)


def ensure_contains(path, needle, message):
    text = read(path)
    if needle not in text:
        raise SystemExit(f"{path}: {message}")
    return text


def patch_console_h():
    path = "include/vita/console.h"
    text = ensure_contains(path, "typedef void (*vita_console_clear_fn)(void);", "missing console clear callback typedef")

    if "vita_console_frame_renderer_fn" not in text:
        text = text.replace(
            "typedef void (*vita_console_clear_fn)(void);\n",
            "typedef void (*vita_console_clear_fn)(void);\n"
            "typedef void (*vita_console_frame_renderer_fn)(bool minimized);\n"
        )

    if "void console_bind_frame_renderer" not in text:
        text = text.replace(
            "void console_bind_clear(vita_console_clear_fn clear_fn);\n",
            "void console_bind_clear(vita_console_clear_fn clear_fn);\n"
            "void console_bind_frame_renderer(vita_console_frame_renderer_fn renderer);\n"
        )

    if "void console_render_frame" not in text:
        text = text.replace(
            "void console_clear_screen(void);\n",
            "void console_clear_screen(void);\n"
            "void console_render_frame(bool minimized);\n"
        )

    write(path, text)


def patch_console_c():
    path = "kernel/console.c"
    text = ensure_contains(path, "static vita_console_clear_fn g_console_clear", "missing console clear storage")

    if "g_console_frame_renderer" not in text:
        text = text.replace(
            "static vita_console_clear_fn g_console_clear = 0;\n",
            "static vita_console_clear_fn g_console_clear = 0;\n"
            "static vita_console_frame_renderer_fn g_console_frame_renderer = 0;\n"
        )

    if "void console_bind_frame_renderer" not in text:
        text = text.replace(
            "void console_bind_clear(vita_console_clear_fn clear_fn) {\n"
            "    g_console_clear = clear_fn;\n"
            "}\n",
            "void console_bind_clear(vita_console_clear_fn clear_fn) {\n"
            "    g_console_clear = clear_fn;\n"
            "}\n\n"
            "void console_bind_frame_renderer(vita_console_frame_renderer_fn renderer) {\n"
            "    g_console_frame_renderer = renderer;\n"
            "}\n"
        )

    if "void console_render_frame" not in text:
        text = text.replace(
            "void console_clear_screen(void) {\n",
            "void console_render_frame(bool minimized) {\n"
            "    if (g_console_frame_renderer) {\n"
            "        g_console_frame_renderer(minimized);\n"
            "    }\n"
            "}\n\n"
            "void console_clear_screen(void) {\n"
        )

    if "console_render_frame(false);" not in text:
        text = text.replace(
            "void console_terminal_show_shell(const vita_console_state_t *state) {\n",
            "void console_terminal_show_shell(const vita_console_state_t *state) {\n"
            "    console_render_frame(false);\n"
        )

    if "console_render_frame(true);" not in text:
        text = text.replace(
            "void console_terminal_show_minimized(void) {\n",
            "void console_terminal_show_minimized(void) {\n"
            "    console_render_frame(true);\n"
        )

    write(path, text)


def patch_uefi_entry_c():
    path = "arch/x86_64/boot/uefi_entry.c"
    text = ensure_contains(path, "#include \"uefi_splash.h\"", "missing uefi_splash include")

    if '#include "uefi_neon_frame.h"' not in text:
        text = text.replace('#include "uefi_splash.h"\n', '#include "uefi_splash.h"\n#include "uefi_neon_frame.h"\n')

    helper = r'''
static void uefi_console_render_frame(bool minimized) {
    vita_uefi_neon_frame_render(g_st, minimized ? 1 : 0);
}
'''
    if "uefi_console_render_frame" not in text:
        marker = "static void uefi_output_raw(const char *text) {"
        text = text.replace(marker, helper + "\n" + marker)

    if "console_bind_frame_renderer(uefi_console_render_frame);" not in text:
        if "console_bind_clear(uefi_console_clear_screen);" in text:
            text = text.replace(
                "    console_bind_clear(uefi_console_clear_screen);\n",
                "    console_bind_clear(uefi_console_clear_screen);\n"
                "    console_bind_frame_renderer(uefi_console_render_frame);\n"
            )
        else:
            text = text.replace(
                "    console_bind_reader(uefi_console_read_line);\n",
                "    console_bind_reader(uefi_console_read_line);\n"
                "    console_bind_frame_renderer(uefi_console_render_frame);\n"
            )

    write(path, text)


def patch_makefile():
    path = "Makefile"
    text = ensure_contains(path, "arch/x86_64/boot/uefi_splash.c", "missing UEFI splash source")

    if "arch/x86_64/boot/uefi_neon_frame.c" not in text:
        text = text.replace(
            "\tarch/x86_64/boot/uefi_splash.c \\\n",
            "\tarch/x86_64/boot/uefi_splash.c \\\n"
            "\tarch/x86_64/boot/uefi_neon_frame.c \\\n"
        )

    write(path, text)


def main():
    patch_console_h()
    patch_console_c()
    patch_uefi_entry_c()
    patch_makefile()
    print("framebuffer neon renderer v17 patch applied")


if __name__ == "__main__":
    main()
