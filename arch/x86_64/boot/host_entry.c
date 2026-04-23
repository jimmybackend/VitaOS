/*
 * arch/x86_64/boot/host_entry.c
 * Hosted boot harness for F1A audit validation.
 */

#include <stdio.h>

#include <vita/boot.h>
#include <vita/console.h>

void kmain(const vita_handoff_t *handoff);

static void host_console_write(const char *text) {
    if (!text) {
        return;
    }
    puts(text);
    fflush(stdout);
}

int main(int argc, char **argv) {
    vita_handoff_t handoff = {0};

    handoff.magic = VITA_BOOT_MAGIC;
    handoff.version = 1;
    handoff.size = sizeof(vita_handoff_t);
    handoff.arch_name = "x86_64";
    handoff.firmware_type = VITA_FIRMWARE_HOSTED;
    handoff.audit_db_path = (argc > 1) ? argv[1] : "build/audit/vitaos-audit.db";
    handoff.vitanet_seed_endpoint = (argc > 2) ? argv[2] : "127.0.0.1:47001";

    console_bind_writer(host_console_write);
    kmain(&handoff);
    return 0;
}
