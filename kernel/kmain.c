/*
 * kernel/kmain.c
 * First executable vertical slice entry + audit gate.
 */

#include <stdbool.h>
#include <stdint.h>

#include <vita/audit.h>
#include <vita/boot.h>
#include <vita/console.h>
#include <vita/hw.h>
#include <vita/proposal.h>

void panic_fatal(const char *reason);

static void mem_zero(void *ptr, unsigned long n) {
    unsigned char *p = (unsigned char *)ptr;
    for (unsigned long i = 0; i < n; ++i) p[i] = 0;
}

static bool handoff_valid(const vita_handoff_t *handoff) {
    if (!handoff) {
        return false;
    }
    if (handoff->magic != VITA_BOOT_MAGIC) {
        return false;
    }
    if (handoff->size < sizeof(vita_handoff_t)) {
        return false;
    }
    return true;
}

static void u64_to_dec(uint64_t v, char out[32]) {
    char tmp[32];
    int i = 0;
    int j = 0;
    if (v == 0) {
        out[0] = '0';
        out[1] = '\0';
        return;
    }
    while (v > 0 && i < (int)sizeof(tmp) - 1) {
        tmp[i++] = (char)('0' + (v % 10));
        v /= 10;
    }
    while (i > 0) {
        out[j++] = tmp[--i];
    }
    out[j] = '\0';
}

static void console_show_hw(const vita_hw_snapshot_t *hw) {
    char num[32];
    console_write_line("HW Snapshot:");
    console_write_line("cpu_arch:");
    console_write_line(hw->cpu_arch[0] ? hw->cpu_arch : "unknown");
    console_write_line("cpu_model:");
    console_write_line(hw->cpu_model[0] ? hw->cpu_model : "unavailable");
    console_write_line("ram_bytes:");
    u64_to_dec(hw->ram_bytes, num);
    console_write_line(num);
    console_write_line("firmware_type:");
    console_write_line(hw->firmware_type[0] ? hw->firmware_type : "unknown");
    console_write_line("console_type:");
    console_write_line(hw->console_type[0] ? hw->console_type : "unknown");
    console_write_line("net_count:");
    u64_to_dec((uint64_t)hw->net_count, num); console_write_line(num);
    console_write_line("storage_count:");
    u64_to_dec((uint64_t)hw->storage_count, num); console_write_line(num);
    console_write_line("usb_count:");
    u64_to_dec((uint64_t)hw->usb_count, num); console_write_line(num);
    console_write_line("wifi_count:");
    u64_to_dec((uint64_t)hw->wifi_count, num); console_write_line(num);
}

void kmain(const vita_handoff_t *handoff) {
    vita_boot_status_t status;
    vita_hw_snapshot_t hw;
    mem_zero(&status, sizeof(status));
    mem_zero(&hw, sizeof(hw));
    status.arch_name = "x86_64";
    status.console_ready = true;
    status.audit_ready = false;

    console_early_init();
    audit_early_buffer_init();
    audit_emit_boot_event("BOOT_STARTED", "boot started");

    if (!handoff_valid(handoff)) {
        panic_fatal("invalid boot handoff");
        return;
    }

    audit_emit_boot_event("HANDOFF_TO_KMAIN", "handoff to kmain");
    audit_emit_boot_event("CONSOLE_READY", "console ready");

    if (handoff->arch_name) {
        status.arch_name = handoff->arch_name;
    }

    if (!audit_init_persistent_backend(handoff)) {
        console_write_line("Audit: FAILED");
        panic_fatal("persistent audit backend required");
        return;
    }

    status.audit_ready = true;
    audit_emit_boot_event("AUDIT_BACKEND_READY", "audit backend ready");

    audit_emit_boot_event("HW_DISCOVERY_STARTED", "hardware discovery started");
    if (hw_discovery_run(handoff, &hw)) {
        audit_emit_boot_event("HW_DISCOVERY_COMPLETED", "hardware discovery completed");
        if (audit_persist_hardware_snapshot(&hw)) {
            audit_emit_boot_event("HW_SNAPSHOT_PERSISTED", "hardware snapshot persisted");
        }
        console_show_hw(&hw);
    }

    proposal_generate_initial(handoff, &hw, status.audit_ready);

    console_banner(&status);
    proposal_show_all();

    if (handoff->firmware_type == VITA_FIRMWARE_HOSTED) {
        proposal_hosted_repl();
        return;
    }

    for (;;) {
        __asm__ __volatile__("hlt");
    }
}
