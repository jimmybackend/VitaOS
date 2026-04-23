/*
 * kernel/kmain.c
 * Entry point for VitaOS common kernel.
 *
 * Responsibilities:
 * - initialize early console
 * - create boot context
 * - drive the state machine
 * - require persistent audit before full operation
 *
 * This file is intentionally a guided skeleton for Codex and contributors.
 */

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    VITA_STATE_BOOTSTRAP = 0,
    VITA_STATE_HW_DISCOVERY,
    VITA_STATE_AUDIT_REQUIRED,
    VITA_STATE_RESTRICTED_DIAGNOSTIC,
    VITA_STATE_OPERATIONAL,
    VITA_STATE_COOPERATIVE
} vita_state_t;

typedef struct {
    const char *arch_name;
    uint64_t ram_bytes;
    bool early_console_ready;
    bool audit_ready;
    bool network_ready;
    bool peers_present;
} boot_context_t;

/* External subsystem entry points. */
void console_early_init(void);
void console_banner(const boot_context_t *ctx);
void panic_fatal(const char *reason);

void memory_early_init(void);
void scheduler_init(void);

void audit_early_buffer_init(void);
bool audit_init_persistent_backend(const boot_context_t *ctx);
void audit_emit_boot_event(const char *event_type, const char *summary);

void ai_core_init(const boot_context_t *ctx);
void proposal_seed_initial_set(const boot_context_t *ctx);

void node_core_init(const boot_context_t *ctx);
bool node_core_has_peers(void);

static void detect_basic_hardware(boot_context_t *ctx) {
    /* TODO: replace placeholders with real x86_64/arm64 discovery hooks. */
    ctx->arch_name = "x86_64";
    ctx->ram_bytes = 0;
    ctx->network_ready = false;
    ctx->peers_present = false;
}

void kmain(void) {
    boot_context_t ctx = {0};

    console_early_init();
    ctx.early_console_ready = true;

    audit_early_buffer_init();
    audit_emit_boot_event("BOOT_START", "VitaOS kernel entry");

    memory_early_init();
    scheduler_init();
    detect_basic_hardware(&ctx);

    audit_emit_boot_event("HW_DISCOVERY_START", "Beginning hardware discovery");

    if (!audit_init_persistent_backend(&ctx)) {
        ctx.audit_ready = false;
        audit_emit_boot_event("AUDIT_MISSING", "Persistent audit backend not available");
        console_banner(&ctx);
        panic_fatal("audit backend required for full operation");
        return;
    }

    ctx.audit_ready = true;
    audit_emit_boot_event("AUDIT_READY", "Persistent audit backend initialized");

    ai_core_init(&ctx);
    node_core_init(&ctx);

    ctx.peers_present = node_core_has_peers();
    proposal_seed_initial_set(&ctx);

    if (ctx.peers_present) {
        audit_emit_boot_event("STATE_COOPERATIVE", "Peers detected during boot");
    } else {
        audit_emit_boot_event("STATE_OPERATIONAL", "System entered operational mode");
    }

    console_banner(&ctx);

    /* TODO:
     * - enter guided console loop
     * - dispatch commands
     * - hand off to emergency_core for free-text requests
     */
    for (;;) {
        /* idle loop */
    }
}
