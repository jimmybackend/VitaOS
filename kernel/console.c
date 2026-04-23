/*
 * kernel/console.c
 * Guided bilingual console for VitaOS.
 *
 * F1 goal:
 * - provide early output
 * - provide a user-facing guided terminal
 * - allow simple commands and free-text emergency descriptions
 */

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    const char *arch_name;
    uint64_t ram_bytes;
    bool early_console_ready;
    bool audit_ready;
    bool network_ready;
    bool peers_present;
} boot_context_t;

static void console_write_line(const char *s) {
    (void)s;
    /* TODO:
     * - connect to framebuffer and/or serial backend
     * - emit UTF-8 safe ASCII subset for F1
     */
}

void console_early_init(void) {
    /* TODO: initialize architecture-specific early console backend. */
}

void console_banner(const boot_context_t *ctx) {
    console_write_line("VitaOS with AI / VitaOS con IA");
    console_write_line("Status / Estado:");
    console_write_line(ctx->audit_ready ? "- Audit SQLite: OK" : "- Audit SQLite: MISSING");
    console_write_line("- AI core: online / IA activa");
    console_write_line("Options / Opciones:");
    console_write_line("1. Emergency assistance / Asistencia de emergencia");
    console_write_line("2. Hardware and system status / Estado de hardware y sistema");
    console_write_line("3. Cooperative nodes / Nodos cooperativos");
    console_write_line("4. Audit review / Revision de auditoria");
    console_write_line("5. Technical console / Consola tecnica");
    console_write_line("Describe what happened or choose an option.");
    (void)ctx;
}

/* TODO for Codex:
 * - add input parser
 * - support `status`, `hw`, `audit`, `peers`, `proposals`, `emergency`
 * - detect user language heuristically
 * - route free-text into emergency_core
 */
