#ifndef VITA_VITAIR_H
#define VITA_VITAIR_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    VITA_TRI_NO = -1,
    VITA_TRI_UNKNOWN = 0,
    VITA_TRI_YES = 1
} vita_tri_t;

typedef enum {
    VITA_IR_SEVERITY_INFO = 0,
    VITA_IR_SEVERITY_WARN,
    VITA_IR_SEVERITY_ERROR,
    VITA_IR_SEVERITY_CRITICAL
} vita_ir_severity_t;

typedef struct {
    const char *ir_version;
    const char *claim;
    vita_tri_t state;
    vita_ir_severity_t severity;
    const char *actor;
    const char *target;
    const char *meaning;
    const char *effect;
} vita_ir_claim_t;

const char *vita_tri_to_symbol(vita_tri_t state);
const char *vita_tri_to_json_number(vita_tri_t state);
const char *vita_tri_to_word(vita_tri_t state);

const char *vita_ir_severity_to_string(vita_ir_severity_t severity);

bool vita_tri_is_valid(vita_tri_t state);
bool vita_ir_severity_is_valid(vita_ir_severity_t severity);

#endif
