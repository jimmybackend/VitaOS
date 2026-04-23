/*
 * kernel/ai_core.c
 * Kernel-resident AI decision core.
 *
 * F1 meaning of "AI in kernel":
 * - perception of state
 * - evaluation of resources and risks
 * - proposal generation
 * - coordination requests for nodes
 *
 * Not in F1:
 * - generic heavy LLM in kernel
 * - unrestricted autonomous execution
 */

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    const char *arch_name;
    uint64_t ram_bytes;
    bool audit_ready;
    bool network_ready;
    bool peers_present;
} ai_context_t;

static ai_context_t g_ai_ctx;

void audit_emit_boot_event(const char *event_type, const char *summary);

void ai_core_init(const void *boot_ctx) {
    (void)boot_ctx;
    /* TODO: copy relevant fields into g_ai_ctx. */
    audit_emit_boot_event("AI_CORE_ONLINE", "AI decision core initialized");
}

/* TODO for Codex:
 * - define ai_observation_t
 * - define ai_proposal_t
 * - implement perception pipeline
 * - implement system proposal templates
 * - integrate with knowledge packs and emergency_core
 * - emit ai_proposal rows through audit_core/sqlite_vfs
 */
