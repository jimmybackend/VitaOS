#!/usr/bin/env python3
"""
VitaOS patch v1: net status command.

Applies a minimal F1A/F1B network readiness command without adding drivers,
DHCP, TCP/IP, Wi-Fi authentication, malloc, or new dependencies.

Run from the repository root:
    python3 tools/patches/apply-net-status-command-v1.py
"""
from pathlib import Path
import sys

ROOT = Path.cwd()
CONSOLE = ROOT / "kernel" / "console.c"
COMMAND = ROOT / "kernel" / "command_core.c"


def fail(msg: str) -> None:
    print(f"[vitaos-net-status] ERROR: {msg}", file=sys.stderr)
    sys.exit(1)


def backup(path: Path) -> None:
    bak = path.with_suffix(path.suffix + ".bak-net-status-v1")
    if not bak.exists():
        bak.write_text(path.read_text())


def patch_console() -> None:
    if not CONSOLE.exists():
        fail(f"missing {CONSOLE}")

    s = CONSOLE.read_text()
    backup(CONSOLE)

    if 'console_write_line("- net");' not in s:
        marker = '    console_write_line("- hw");\n'
        if marker not in s:
            fail("could not find hw menu marker in kernel/console.c")
        s = s.replace(marker, marker + '    console_write_line("- net");\n', 1)

    help_line = '    console_write_line("net         -> show network readiness summary");\n'
    if help_line not in s:
        marker = '    console_write_line("hw          -> show hardware summary when available");\n'
        if marker not in s:
            fail("could not find hw help marker in kernel/console.c")
        s = s.replace(marker, marker + help_line, 1)

    CONSOLE.write_text(s)
    print("[vitaos-net-status] patched kernel/console.c")


SHOW_NET = r'''
static void show_net(const vita_command_context_t *ctx) {
    const vita_hw_snapshot_t *hw;

    console_write_line("Network / Red:");

    if (!ctx || !ctx->hw_ready) {
        console_write_line("No hardware snapshot available / No hay resumen de hardware disponible");
        console_write_line("Network readiness cannot be evaluated yet.");
        console_write_line("La disponibilidad de red aun no se puede evaluar.");
        return;
    }

    hw = &ctx->hw_snapshot;

    console_write_i32("net_count:", hw->net_count);
    console_write_i32("ethernet_count:", hw->ethernet_count);
    console_write_i32("wifi_count:", hw->wifi_count);
    console_write_i32("usb_controller_count:", hw->usb_controller_count);

    console_write_line("Readiness / Preparacion:");

    if (hw->net_count > 0 || hw->ethernet_count > 0 || hw->wifi_count > 0) {
        console_write_line("- Network hardware/protocol presence detected.");
        console_write_line("- Se detecto presencia de hardware/protocolo de red.");
    } else {
        console_write_line("- No network interface/protocol was detected in this boot.");
        console_write_line("- No se detecto interfaz/protocolo de red en este arranque.");
    }

    if (hw->ethernet_count > 0) {
        console_write_line("- Ethernet appears available for a future minimal connection path.");
        console_write_line("- Ethernet parece disponible para una ruta minima futura de conexion.");
    } else {
        console_write_line("- Ethernet not visible yet / Ethernet aun no visible.");
    }

    if (hw->wifi_count > 0) {
        console_write_line("- Wi-Fi hardware is visible, but Wi-Fi association/authentication is not implemented yet.");
        console_write_line("- Wi-Fi es visible, pero asociacion/autenticacion Wi-Fi aun no esta implementada.");
    } else {
        console_write_line("- Wi-Fi not visible yet / Wi-Fi aun no visible.");
    }

    if (ctx->handoff && ctx->handoff->firmware_type == VITA_FIRMWARE_UEFI) {
        console_write_line("UEFI mode / Modo UEFI:");
        console_write_line("- This command reports readiness only; it does not run DHCP, TCP/IP, DNS, TLS, or Wi-Fi login yet.");
        console_write_line("- Este comando solo reporta preparacion; aun no ejecuta DHCP, TCP/IP, DNS, TLS ni login Wi-Fi.");
    } else {
        console_write_line("Hosted mode / Modo hosted:");
        console_write_line("- Network counts come from the host OS view and are used for validation.");
        console_write_line("- Los conteos de red vienen del sistema anfitrion y sirven para validacion.");
    }

    console_write_line("Next planned steps / Siguientes pasos planeados:");
    console_write_line("- net dhcp       -> future Ethernet DHCP attempt");
    console_write_line("- net config     -> future manual IP setup");
    console_write_line("- ai connect     -> future remote assistant gateway");
}

'''

NET_HANDLER = r'''    if (str_eq(cmd, "net") || str_eq(cmd, "net status")) {
        audit_emit_boot_event("COMMAND_NET_STATUS", "net status");
        show_net(ctx);
        return VITA_COMMAND_CONTINUE;
    }

'''


def patch_command_core() -> None:
    if not COMMAND.exists():
        fail(f"missing {COMMAND}")

    s = COMMAND.read_text()
    backup(COMMAND)

    if "static void show_net(" not in s:
        marker = "static void show_audit(const vita_command_context_t *ctx) {"
        if marker not in s:
            fail("could not find show_audit marker in kernel/command_core.c")
        s = s.replace(marker, SHOW_NET + marker, 1)

    if 'COMMAND_NET_STATUS' not in s:
        marker = '''    if (str_eq(cmd, "audit")) {
        audit_emit_boot_event("COMMAND_AUDIT", "audit");
        show_audit(ctx);
        return VITA_COMMAND_CONTINUE;
    }

'''
        if marker not in s:
            fail("could not find audit command marker in kernel/command_core.c")
        s = s.replace(marker, NET_HANDLER + marker, 1)

    COMMAND.write_text(s)
    print("[vitaos-net-status] patched kernel/command_core.c")


def main() -> None:
    if not (ROOT / "Makefile").exists() or not (ROOT / "kernel").exists():
        fail("run this script from the VitaOS repository root")

    patch_console()
    patch_command_core()
    print("[vitaos-net-status] done")
    print("[vitaos-net-status] recommended test:")
    print("  make clean && make hosted && make smoke-audit && make && make iso")


if __name__ == "__main__":
    main()
