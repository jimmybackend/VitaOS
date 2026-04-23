/*
 * kernel/audit_core.c
 * Audit-first subsystem.
 *
 * Responsibilities:
 * - early volatile audit buffer
 * - persistent SQLite backend
 * - append-mostly event flow
 * - hash chain generation
 */

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    bool persistent_ready;
    uint64_t event_seq;
} audit_state_t;

static audit_state_t g_audit = {0};

void audit_early_buffer_init(void) {
    g_audit.persistent_ready = false;
    g_audit.event_seq = 0;
}

bool audit_init_persistent_backend(const void *ctx) {
    (void)ctx;
    /* TODO:
     * - discover writable partition on live USB
     * - open SQLite backend via sqlite_vfs
     * - apply schema if needed
     * - persist boot_session
     */
    return false;
}

void audit_emit_boot_event(const char *event_type, const char *summary) {
    (void)event_type;
    (void)summary;
    g_audit.event_seq++;

    /* TODO:
     * - compute hash chain
     * - write to SQLite if persistent backend is ready
     * - otherwise store in early buffer
     */
}

/* TODO:
 * - audit_emit_event(...)
 * - audit_flush_buffer()
 * - audit_export_jsonl()
 * - audit_validate_chain()
 */
