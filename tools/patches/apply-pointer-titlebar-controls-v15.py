#!/usr/bin/env python3
from pathlib import Path

ROOT = Path.cwd()


def read(path: str) -> str:
    return (ROOT / path).read_text()


def write(path: str, text: str) -> None:
    (ROOT / path).write_text(text)


def ensure_contains(path: str, needle: str) -> str:
    text = read(path)
    if needle not in text:
        raise SystemExit(f"[v15] expected marker not found in {path}: {needle!r}")
    return text


def insert_after_once(path: str, marker: str, insertion: str, token: str) -> None:
    text = ensure_contains(path, marker)
    if token in text:
        print(f"[v15] {path}: already patched ({token})")
        return
    text = text.replace(marker, marker + insertion, 1)
    write(path, text)
    print(f"[v15] patched {path}")


def insert_before_once(path: str, marker: str, insertion: str, token: str) -> None:
    text = ensure_contains(path, marker)
    if token in text:
        print(f"[v15] {path}: already patched ({token})")
        return
    text = text.replace(marker, insertion + marker, 1)
    write(path, text)
    print(f"[v15] patched {path}")


def replace_once(path: str, old: str, new: str, token: str) -> None:
    text = read(path)
    if token in text:
        print(f"[v15] {path}: already patched ({token})")
        return
    if old not in text:
        raise SystemExit(f"[v15] expected replacement marker not found in {path}: {old!r}")
    text = text.replace(old, new, 1)
    write(path, text)
    print(f"[v15] patched {path}")


def patch_uefi_entry() -> None:
    path = "arch/x86_64/boot/uefi_entry.c"

    insertion_types = r'''

/* VITA_PATCH_V15_POINTER_TYPES_BEGIN
 * Optional UEFI pointer support for the staged neon title bar controls.
 * This is deliberately conservative: if firmware does not expose a pointer
 * protocol on ConsoleInHandle, keyboard-only operation continues unchanged.
 */
typedef struct efi_simple_pointer_protocol efi_simple_pointer_protocol_t;
typedef struct efi_absolute_pointer_protocol efi_absolute_pointer_protocol_t;

typedef struct {
    uint32_t data1;
    uint16_t data2;
    uint16_t data3;
    uint8_t data4[8];
} efi_guid_t;

typedef struct {
    uint64_t resolution_x;
    uint64_t resolution_y;
    uint64_t resolution_z;
    uint8_t left_button;
    uint8_t right_button;
} efi_simple_pointer_mode_t;

typedef struct {
    int32_t relative_movement_x;
    int32_t relative_movement_y;
    int32_t relative_movement_z;
    uint8_t left_button;
    uint8_t right_button;
} efi_simple_pointer_state_t;

struct efi_simple_pointer_protocol {
    efi_status_t (*reset)(efi_simple_pointer_protocol_t *self, uint8_t extended_verification);
    efi_status_t (*get_state)(efi_simple_pointer_protocol_t *self, efi_simple_pointer_state_t *state);
    efi_event_t wait_for_input;
    efi_simple_pointer_mode_t *mode;
};

typedef struct {
    uint64_t absolute_min_x;
    uint64_t absolute_min_y;
    uint64_t absolute_min_z;
    uint64_t absolute_max_x;
    uint64_t absolute_max_y;
    uint64_t absolute_max_z;
    uint32_t attributes;
} efi_absolute_pointer_mode_t;

typedef struct {
    uint64_t current_x;
    uint64_t current_y;
    uint64_t current_z;
    uint32_t active_buttons;
} efi_absolute_pointer_state_t;

struct efi_absolute_pointer_protocol {
    efi_status_t (*reset)(efi_absolute_pointer_protocol_t *self, uint8_t extended_verification);
    efi_status_t (*get_state)(efi_absolute_pointer_protocol_t *self, efi_absolute_pointer_state_t *state);
    efi_event_t wait_for_input;
    efi_absolute_pointer_mode_t *mode;
};
/* VITA_PATCH_V15_POINTER_TYPES_END */
'''
    insert_before_once(path, "void kmain(const vita_handoff_t *handoff);", insertion_types, "VITA_PATCH_V15_POINTER_TYPES_BEGIN")

    # Extend boot services only when the current struct still has handle_protocol.
    # No extra Boot Services entries are required because this patch uses HandleProtocol on ConsoleInHandle.

    insertion_globals = r'''

/* VITA_PATCH_V15_POINTER_GLOBALS_BEGIN */
static efi_simple_pointer_protocol_t *g_simple_pointer = 0;
static efi_absolute_pointer_protocol_t *g_absolute_pointer = 0;
static int32_t g_pointer_col = 0;
static int32_t g_pointer_row = 0;
static uint8_t g_pointer_prev_left = 0;
static uint8_t g_pointer_prev_abs_left = 0;

static efi_guid_t g_simple_pointer_protocol_guid = {
    0x31878c87U, 0x0b75U, 0x11d5U,
    {0x9aU, 0x4fU, 0x00U, 0x90U, 0x27U, 0x3fU, 0xc1U, 0x4dU}
};

static efi_guid_t g_absolute_pointer_protocol_guid = {
    0x8d59d32bU, 0xc655U, 0x4ae9U,
    {0x9bU, 0x15U, 0xf2U, 0x59U, 0x04U, 0x99U, 0x2aU, 0x43U}
};
/* VITA_PATCH_V15_POINTER_GLOBALS_END */
'''
    insert_after_once(path, "static efi_system_table_t *g_st = 0;\n", insertion_globals, "VITA_PATCH_V15_POINTER_GLOBALS_BEGIN")

    insertion_functions = r'''

/* VITA_PATCH_V15_POINTER_FUNCTIONS_BEGIN */
static int32_t vita_abs_i32(int32_t v) {
    return (v < 0) ? -v : v;
}

static void vita_copy_command(char *out, unsigned long out_cap, const char *cmd) {
    unsigned long i = 0;

    if (!out || out_cap == 0) {
        return;
    }

    out[0] = '\0';
    if (!cmd) {
        return;
    }

    while (cmd[i] && (i + 1) < out_cap) {
        out[i] = cmd[i];
        i++;
    }
    out[i] = '\0';
}

static int32_t vita_clamp_i32(int32_t value, int32_t lo, int32_t hi) {
    if (value < lo) {
        return lo;
    }
    if (value > hi) {
        return hi;
    }
    return value;
}

static void uefi_pointer_init(void) {
    efi_boot_services_t *bs;
    void *iface = 0;

    if (!g_st || !g_st->boot_services || !g_st->console_in_handle) {
        return;
    }

    bs = g_st->boot_services;
    if (!bs->handle_protocol) {
        return;
    }

    if (bs->handle_protocol(g_st->console_in_handle,
                            &g_simple_pointer_protocol_guid,
                            &iface) == EFI_SUCCESS && iface) {
        g_simple_pointer = (efi_simple_pointer_protocol_t *)iface;
        if (g_simple_pointer->reset) {
            (void)g_simple_pointer->reset(g_simple_pointer, 0);
        }
    }

    iface = 0;
    if (bs->handle_protocol(g_st->console_in_handle,
                            &g_absolute_pointer_protocol_guid,
                            &iface) == EFI_SUCCESS && iface) {
        g_absolute_pointer = (efi_absolute_pointer_protocol_t *)iface;
        if (g_absolute_pointer->reset) {
            (void)g_absolute_pointer->reset(g_absolute_pointer, 0);
        }
    }

    /* Start near the title bar controls on a conventional 80x25 firmware console. */
    g_pointer_col = 68;
    g_pointer_row = 1;
}

static const char *uefi_titlebar_command_from_position(int32_t col, int32_t row) {
    if (row < 0 || row > 3) {
        return 0;
    }

    /* Approximate control zones matching the staged title bar text:
     * [MIN] [RESTART] [POWER]
     */
    if (col >= 44 && col <= 51) {
        return "minimize";
    }
    if (col >= 53 && col <= 64) {
        return "restart";
    }
    if (col >= 66 && col <= 79) {
        return "shutdown";
    }

    return 0;
}

static bool uefi_poll_absolute_pointer_command(char *out, unsigned long out_cap) {
    efi_absolute_pointer_state_t state;
    efi_absolute_pointer_mode_t *mode;
    uint8_t left;
    const char *cmd;
    uint64_t dx;
    uint64_t dy;

    if (!g_absolute_pointer || !g_absolute_pointer->get_state) {
        return false;
    }

    state.current_x = 0;
    state.current_y = 0;
    state.current_z = 0;
    state.active_buttons = 0;

    if (g_absolute_pointer->get_state(g_absolute_pointer, &state) != EFI_SUCCESS) {
        return false;
    }

    mode = g_absolute_pointer->mode;
    if (mode && mode->absolute_max_x > mode->absolute_min_x) {
        dx = mode->absolute_max_x - mode->absolute_min_x;
        g_pointer_col = (int32_t)(((state.current_x - mode->absolute_min_x) * 80ULL) / dx);
    }
    if (mode && mode->absolute_max_y > mode->absolute_min_y) {
        dy = mode->absolute_max_y - mode->absolute_min_y;
        g_pointer_row = (int32_t)(((state.current_y - mode->absolute_min_y) * 25ULL) / dy);
    }

    g_pointer_col = vita_clamp_i32(g_pointer_col, 0, 79);
    g_pointer_row = vita_clamp_i32(g_pointer_row, 0, 24);

    left = (state.active_buttons & 1U) ? 1U : 0U;
    if (left && !g_pointer_prev_abs_left) {
        cmd = uefi_titlebar_command_from_position(g_pointer_col, g_pointer_row);
        if (cmd) {
            uefi_output_raw("\r\n[pointer/control] ");
            uefi_output_raw(cmd);
            uefi_output_raw("\r\n");
            vita_copy_command(out, out_cap, cmd);
            g_pointer_prev_abs_left = left;
            return true;
        }
    }

    g_pointer_prev_abs_left = left;
    return false;
}

static bool uefi_poll_simple_pointer_command(char *out, unsigned long out_cap) {
    efi_simple_pointer_state_t state;
    int32_t step_x;
    int32_t step_y;
    uint8_t left;
    const char *cmd;

    if (!g_simple_pointer || !g_simple_pointer->get_state) {
        return false;
    }

    state.relative_movement_x = 0;
    state.relative_movement_y = 0;
    state.relative_movement_z = 0;
    state.left_button = 0;
    state.right_button = 0;

    if (g_simple_pointer->get_state(g_simple_pointer, &state) != EFI_SUCCESS) {
        return false;
    }

    step_x = state.relative_movement_x / 250;
    step_y = state.relative_movement_y / 250;

    if (step_x == 0 && vita_abs_i32(state.relative_movement_x) > 0) {
        step_x = (state.relative_movement_x > 0) ? 1 : -1;
    }
    if (step_y == 0 && vita_abs_i32(state.relative_movement_y) > 0) {
        step_y = (state.relative_movement_y > 0) ? 1 : -1;
    }

    g_pointer_col = vita_clamp_i32(g_pointer_col + step_x, 0, 79);
    g_pointer_row = vita_clamp_i32(g_pointer_row + step_y, 0, 24);

    left = state.left_button ? 1U : 0U;
    if (left && !g_pointer_prev_left) {
        cmd = uefi_titlebar_command_from_position(g_pointer_col, g_pointer_row);
        if (cmd) {
            uefi_output_raw("\r\n[pointer/control] ");
            uefi_output_raw(cmd);
            uefi_output_raw("\r\n");
            vita_copy_command(out, out_cap, cmd);
            g_pointer_prev_left = left;
            return true;
        }
    }

    g_pointer_prev_left = left;
    return false;
}

static bool uefi_poll_pointer_control_command(char *out, unsigned long out_cap) {
    if (uefi_poll_absolute_pointer_command(out, out_cap)) {
        return true;
    }

    return uefi_poll_simple_pointer_command(out, out_cap);
}
/* VITA_PATCH_V15_POINTER_FUNCTIONS_END */
'''
    # Insert after uefi_try_read_key function block using the next function marker.
    insert_before_once(path, "static bool uefi_allowed_input_char(char c) {", insertion_functions, "VITA_PATCH_V15_POINTER_FUNCTIONS_BEGIN")

    old = """        if (!uefi_try_read_key(&key)) {\n            idle_polls++;\n"""
    new = """        if (!uefi_try_read_key(&key)) {\n            if (uefi_poll_pointer_control_command(out, out_cap)) {\n                return true;\n            }\n\n            idle_polls++;\n"""
    replace_once(path, old, new, "uefi_poll_pointer_control_command(out, out_cap)")

    old_main = """    console_bind_reader(uefi_console_read_line);\n\n    /*\n"""
    new_main = """    console_bind_reader(uefi_console_read_line);\n    uefi_pointer_init();\n\n    /*\n"""
    replace_once(path, old_main, new_main, "uefi_pointer_init();")


def patch_console_c() -> None:
    path = "kernel/console.c"
    text = read(path)

    if "Click [MIN], [RESTART], or [POWER]" not in text:
        text = text.replace(
            '    console_write_line("Keys / Teclas: Up/Down history in UEFI, Enter continues paging.");\n',
            '    console_write_line("Keys / Teclas: Up/Down history in UEFI, Enter continues paging.");\n'
            '    console_write_line("Pointer / Mouse: Click [MIN], [RESTART], or [POWER] when firmware exposes a pointer protocol.");\n',
            1,
        )
    if 'console_write_line("- restart");' not in text:
        text = text.replace(
            '    console_write_line("- clear");\n',
            '    console_write_line("- clear");\n'
            '    console_write_line("- minimize / restore / maximize");\n'
            '    console_write_line("- restart");\n',
            1,
        )
    write(path, text)
    print(f"[v15] patched {path}")


def patch_command_core() -> None:
    path = "kernel/command_core.c"
    insertion = r'''

    /* VITA_PATCH_V15_RESTART_COMMAND_BEGIN */
    if (str_eq(cmd, "restart") || str_eq(cmd, "reboot")) {
        audit_emit_boot_event("COMMAND_RESTART_REQUESTED", "restart");
        console_write_line("Restart requested / Reinicio solicitado");
        console_write_line("Safe restart is staged for the next power-flow patch.");
        console_write_line("Use shutdown for now, then restart the machine manually.");
        return VITA_COMMAND_CONTINUE;
    }
    /* VITA_PATCH_V15_RESTART_COMMAND_END */
'''
    insert_before_once(
        path,
        '    if (str_eq(cmd, "shutdown") || str_eq(cmd, "exit")) {',
        insertion,
        "VITA_PATCH_V15_RESTART_COMMAND_BEGIN",
    )


def main() -> None:
    patch_uefi_entry()
    patch_console_c()
    patch_command_core()
    print("[v15] pointer title bar controls patch applied")


if __name__ == "__main__":
    main()
