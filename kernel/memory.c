/*
 * kernel/memory.c
 * Early memory initialization and future heap contract.
 *
 * F1 scope:
 * - detect usable memory ranges
 * - initialize a minimal allocator contract
 * - provide enough memory services for audit, console and AI core
 */

#include <stdint.h>

typedef struct {
    uint64_t base;
    uint64_t length;
    uint32_t type;
} vita_mem_region_t;

void memory_early_init(void) {
    /* TODO:
     * - parse memory map from arch handoff
     * - reserve kernel image
     * - reserve boot structures
     * - reserve early audit buffers
     */
}

/* TODO for Codex:
 * - define page allocator
 * - define bootstrap heap
 * - document ownership rules
 * - audit major memory state transitions if they affect operation
 */
