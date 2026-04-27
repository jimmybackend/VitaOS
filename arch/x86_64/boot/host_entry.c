/*
 * arch/x86_64/boot/host_entry.c
 * Hosted boot harness for F1A/F1B validation.
 */

#include <stdio.h>
#include <stdlib.h>

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

static bool host_console_read_line(char *out, unsigned long out_cap) {
    unsigned long n = 0;

    if (!out || out_cap == 0) {
        return false;
    }

    out[0] = '\0';

    if (!fgets(out, (int)out_cap, stdin)) {
        return false;
    }

    while (out[n]) {
        n++;
    }

    while (n > 0 && (out[n - 1] == '\n' || out[n - 1] == '\r')) {
        out[--n] = '\0';
    }

    return true;
}

int main(int argc, char **argv) {
    vita_handoff_t handoff = {0};
    const char *seed_endpoint = NULL;

    handoff.magic = VITA_BOOT_MAGIC;
    handoff.version = 1;
    handoff.size = sizeof(vita_handoff_t);
    handoff.arch_name = "x86_64";
    handoff.firmware_type = VITA_FIRMWARE_HOSTED;
    handoff.audit_db_path = (argc > 1) ? argv[1] : "build/audit/vitaos-audit.db";

    seed_endpoint = getenv("VITANET_SEED");
    handoff.vitanet_seed_endpoint = (seed_endpoint && seed_endpoint[0])
        ? seed_endpoint
        : "127.0.0.1:47001";

    handoff.uefi_image_handle = 0;
    handoff.uefi_system_table = 0;

    console_bind_writer(host_console_write);
    console_bind_reader(host_console_read_line);
    kmain(&handoff);
    return 0;
}
