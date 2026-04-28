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
    text = ensure_contains(path, "void console_guided_prompt(void);", "missing console_guided_prompt declaration")

    decl = """
void console_terminal_show_shell(const vita_console_state_t *state);
void console_terminal_show_minimized(void);
void console_terminal_set_minimized(bool minimized);
bool console_terminal_is_minimized(void);
"""
    if "console_terminal_show_shell" not in text:
        text = text.replace("void console_guided_prompt(void);\n", "void console_guided_prompt(void);\n" + decl)

    write(path, text)


def patch_console_c():
    path = "kernel/console.c"
    text = ensure_contains(path, "static const char *mode_name", "missing mode_name helper")

    if "g_terminal_panel_minimized" not in text:
        text = text.replace(
            "static unsigned long g_pager_line_limit = VITA_CONSOLE_PAGE_LINES_DEFAULT;\n",
            "static unsigned long g_pager_line_limit = VITA_CONSOLE_PAGE_LINES_DEFAULT;\n"
            "static bool g_terminal_panel_minimized = false;\n"
        )

    helper = r'''
static void console_write_centered80(const char *text) {
    unsigned long len = 0;
    unsigned long pad;
    char line[96];
    unsigned long i = 0;
    unsigned long j = 0;

    if (!text) {
        console_write_line("");
        return;
    }

    while (text[len]) {
        len++;
    }

    pad = (len < 78U) ? ((78U - len) / 2U) : 0U;

    while (i < pad && (j + 1U) < sizeof(line)) {
        line[j++] = ' ';
        i++;
    }

    i = 0;
    while (text[i] && (j + 1U) < sizeof(line)) {
        line[j++] = text[i++];
    }

    line[j] = '\0';
    console_write_line(line);
}

void console_terminal_set_minimized(bool minimized) {
    g_terminal_panel_minimized = minimized;
}

bool console_terminal_is_minimized(void) {
    return g_terminal_panel_minimized;
}

void console_terminal_show_minimized(void) {
    g_terminal_panel_minimized = true;
    console_clear_screen();
    console_write_line("+------------------------------------------------------------------------------+");
    console_write_line("| VitaOS Terminal minimized / Terminal VitaOS minimizada        [RESTORE]      |");
    console_write_line("+------------------------------------------------------------------------------+");
    console_write_line("Type restore or maximize to reopen / Escribe restore o maximize para restaurar");
}

void console_terminal_show_shell(const vita_console_state_t *state) {
    const char *audit_text = "audit: unknown";
    const char *mode_text = "mode: unknown";

    if (state) {
        audit_text = state->audit_ready ? "audit: ready" : "audit: restricted";
        mode_text = state->boot_mode ? state->boot_mode : "mode: unknown";
    }

    g_terminal_panel_minimized = false;

    console_write_line("+------------------------------------------------------------------------------+");
    console_write_line("| VitaOS Terminal                                      [MIN] [RESTART] [POWER] |");
    console_write_line("+------------------------------------------------------------------------------+");
    console_write_centered80("VitaOS with AI / VitaOS con IA");
    console_write_centered80("centered neon terminal shell / terminal neon centrada");
    console_write_line("+------------------------------------------------------------------------------+");
    console_write_line(audit_text);
    console_write_line(mode_text);
    console_write_line("Controls / Controles: minimize | restore | restart* | shutdown");
    console_write_line("*restart/power icons are staged until safe power flow is implemented.");
    console_write_line("------------------------------------------------------------------------------");
}
'''

    if "void console_terminal_show_shell" not in text:
        marker = "static void console_write_kv(const char *key, const char *value) {"
        text = text.replace(marker, helper + "\n" + marker)

    # Add menu entries if not present
    if 'console_write_line("- minimize")' not in text:
        text = text.replace(
            '    console_write_line("- clear");\n',
            '    console_write_line("- clear");\n'
            '    console_write_line("- minimize");\n'
            '    console_write_line("- restore");\n'
        )

    if 'console_write_line("minimize   -> collapse terminal panel")' not in text:
        text = text.replace(
            '    console_write_line("clear       -> clear screen and redraw guided header");\n',
            '    console_write_line("clear       -> clear screen and redraw guided header");\n'
            '    console_write_line("minimize   -> collapse terminal panel");\n'
            '    console_write_line("restore    -> restore/maximize terminal panel");\n'
        )

    # Insert shell frame at beginning of welcome, after function opening brace.
    if "console_terminal_show_shell(state);" not in text:
        text = text.replace(
            "void console_guided_show_welcome(const vita_console_state_t *state) {\n",
            "void console_guided_show_welcome(const vita_console_state_t *state) {\n"
            "    if (!console_terminal_is_minimized()) {\n"
            "        console_terminal_show_shell(state);\n"
            "    }\n"
        )

    write(path, text)


def patch_command_core_c():
    path = "kernel/command_core.c"
    text = ensure_contains(path, "if (str_eq(cmd, \"clear\")", "missing clear command handler")

    block = r'''
    if (str_eq(cmd, "minimize") || str_eq(cmd, "min")) {
        audit_emit_boot_event("COMMAND_MINIMIZE", "minimize");
        console_terminal_show_minimized();
        return VITA_COMMAND_CONTINUE;
    }

    if (str_eq(cmd, "restore") || str_eq(cmd, "maximize") || str_eq(cmd, "max")) {
        audit_emit_boot_event("COMMAND_RESTORE", "restore");
        console_clear_screen();
        command_refresh_state(ctx);
        console_terminal_show_shell(&ctx->console_state);
        console_guided_show_menu();
        return VITA_COMMAND_CONTINUE;
    }

'''
    if "COMMAND_MINIMIZE" not in text:
        text = text.replace("    if (starts_with(cmd, \"approve \") || starts_with(cmd, \"reject \")) {\n", block + "    if (starts_with(cmd, \"approve \") || starts_with(cmd, \"reject \")) {\n")

    write(path, text)


def patch_uefi_entry_c():
    path = "arch/x86_64/boot/uefi_entry.c"
    text = ensure_contains(path, "struct efi_simple_text_output_protocol", "missing UEFI text output protocol")

    if "EFI_TEXT_ATTR" not in text:
        text = text.replace(
            "#define EFI_SUCCESS 0ULL\n",
            "#define EFI_SUCCESS 0ULL\n"
            "#define EFI_BLACK 0x00U\n"
            "#define EFI_LIGHTBLUE 0x09U\n"
            "#define EFI_LIGHTCYAN 0x0BU\n"
            "#define EFI_TEXT_ATTR(fg, bg) ((uint64_t)((fg) | ((bg) << 4U)))\n"
        )

    # Replace short Simple Text Output protocol with complete early subset if needed.
    if "set_attribute" not in text or "clear_screen" not in text:
        old = re.search(r"struct efi_simple_text_output_protocol \{.*?\};", text, flags=re.S)
        if not old:
            raise SystemExit("arch/x86_64/boot/uefi_entry.c: could not locate simple text output protocol struct")
        new = """struct efi_simple_text_output_protocol {
    efi_status_t (*reset)(efi_simple_text_output_protocol_t *self, uint8_t extended_verification);
    efi_status_t (*output_string)(efi_simple_text_output_protocol_t *self, char16_t *string);
    efi_status_t (*test_string)(efi_simple_text_output_protocol_t *self, char16_t *string);
    efi_status_t (*query_mode)(efi_simple_text_output_protocol_t *self,
                               uint64_t mode_number,
                               uint64_t *columns,
                               uint64_t *rows);
    efi_status_t (*set_mode)(efi_simple_text_output_protocol_t *self, uint64_t mode_number);
    efi_status_t (*set_attribute)(efi_simple_text_output_protocol_t *self, uint64_t attribute);
    efi_status_t (*clear_screen)(efi_simple_text_output_protocol_t *self);
    efi_status_t (*set_cursor_position)(efi_simple_text_output_protocol_t *self,
                                        uint64_t column,
                                        uint64_t row);
    efi_status_t (*enable_cursor)(efi_simple_text_output_protocol_t *self, uint8_t visible);
    void *mode;
};"""
        text = text[:old.start()] + new + text[old.end():]

    helper = r'''
static void uefi_console_apply_neon_theme(void) {
    if (!g_st || !g_st->con_out || !g_st->con_out->set_attribute) {
        return;
    }

    /* Cyan/blue on black: closest Simple Text Output equivalent to neon. */
    (void)g_st->con_out->set_attribute(g_st->con_out, EFI_TEXT_ATTR(EFI_LIGHTCYAN, EFI_BLACK));
}

static void uefi_console_clear_screen(void) {
    if (!g_st || !g_st->con_out || !g_st->con_out->clear_screen) {
        return;
    }

    (void)g_st->con_out->clear_screen(g_st->con_out);
}
'''
    if "uefi_console_apply_neon_theme" not in text:
        text = text.replace("static bool uefi_console_read_line", helper + "\nstatic bool uefi_console_read_line")

    # If console_bind_clear exists in current repo, bind it. Otherwise harmless to skip.
    if "console_bind_clear" in read("include/vita/console.h") and "console_bind_clear(uefi_console_clear_screen);" not in text:
        text = text.replace(
            "    console_bind_reader(uefi_console_read_line);\n",
            "    console_bind_reader(uefi_console_read_line);\n"
            "    console_bind_clear(uefi_console_clear_screen);\n"
        )

    if "uefi_console_apply_neon_theme();" not in text:
        text = text.replace(
            "    vita_uefi_show_splash(image_handle, system_table);\n",
            "    vita_uefi_show_splash(image_handle, system_table);\n"
            "    uefi_console_clear_screen();\n"
            "    uefi_console_apply_neon_theme();\n"
        )

    write(path, text)


def main():
    patch_console_h()
    patch_console_c()
    patch_command_core_c()
    patch_uefi_entry_c()
    print("neon terminal shell v14 patch applied")


if __name__ == "__main__":
    main()
