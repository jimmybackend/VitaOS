#ifndef VITA_VITAIR_AUDIT_H
#define VITA_VITAIR_AUDIT_H

#include <stddef.h>

#include <vita/console.h>
#include <vita/vitair.h>

#define VITA_IR_VERSION "vitair-tri/0.1"
#define VITA_IR_AUDIT_CLAIM_MAX 8U

size_t vita_ir_claims_from_audit_runtime(const vita_audit_runtime_status_t *runtime,
                                         vita_ir_claim_t *out_claims,
                                         size_t max_claims);

#endif
