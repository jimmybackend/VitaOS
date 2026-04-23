/*
 * kernel/scheduler.c
 * Minimal scheduler skeleton.
 *
 * F1 does not require a complex multitasking model.
 * It only needs enough structure for:
 * - console loop
 * - audit flush task
 * - AI proposal engine tick
 * - VitaNet heartbeat
 */

#include <stdint.h>

typedef void (*vita_task_fn_t)(void *);

typedef struct {
    const char *name;
    vita_task_fn_t entry;
    void *arg;
    uint32_t period_ms;
} vita_task_t;

void scheduler_init(void) {
    /* TODO:
     * - initialize task list
     * - register minimal periodic jobs
     */
}

/* TODO:
 * - scheduler_register(vita_task_t*)
 * - scheduler_tick()
 * - idle task
 * - audit events for task start/failure if useful
 */
