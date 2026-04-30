#include <vita/vitair_audit.h>

static void map_status_to_claim(vita_status_t status, vita_tri_t *state, vita_ir_severity_t *severity) {
    if (!state || !severity) {
        return;
    }

    switch (status) {
        case VITA_STATUS_OK:
            *state = VITA_TRI_YES;
            *severity = VITA_IR_SEVERITY_INFO;
            break;
        case VITA_STATUS_FAIL:
            *state = VITA_TRI_NO;
            *severity = VITA_IR_SEVERITY_ERROR;
            break;
        case VITA_STATUS_UNAVAILABLE:
            *state = VITA_TRI_NO;
            *severity = VITA_IR_SEVERITY_WARN;
            break;
        case VITA_STATUS_WARN:
        case VITA_STATUS_UNKNOWN:
        default:
            *state = VITA_TRI_UNKNOWN;
            *severity = VITA_IR_SEVERITY_WARN;
            break;
    }
}

static bool append_claim(vita_ir_claim_t *out_claims,
                         size_t max_claims,
                         size_t *count,
                         const char *claim,
                         vita_tri_t state,
                         vita_ir_severity_t severity,
                         const char *actor,
                         const char *target,
                         const char *meaning,
                         const char *effect) {
    vita_ir_claim_t *entry;

    if (!out_claims || !count || !claim || *count >= max_claims) {
        return false;
    }

    entry = &out_claims[*count];
    entry->ir_version = VITA_IR_VERSION;
    entry->claim = claim;
    entry->state = state;
    entry->severity = severity;
    entry->actor = actor;
    entry->target = target;
    entry->meaning = meaning;
    entry->effect = effect;
    *count = *count + 1U;
    return true;
}

size_t vita_ir_claims_from_audit_runtime(const vita_audit_runtime_status_t *runtime,
                                         vita_ir_claim_t *out_claims,
                                         size_t max_claims) {
    size_t count = 0;
    vita_tri_t state = VITA_TRI_UNKNOWN;
    vita_ir_severity_t severity = VITA_IR_SEVERITY_WARN;

    if (!runtime || !out_claims || max_claims == 0U) {
        return 0;
    }

    append_claim(out_claims,
                 max_claims,
                 &count,
                 "storage.persistent.writable",
                 runtime->persistent_storage_writable ? VITA_TRI_YES : VITA_TRI_NO,
                 runtime->persistent_storage_writable ? VITA_IR_SEVERITY_INFO : VITA_IR_SEVERITY_WARN,
                 "storage",
                 "/vita",
                 runtime->persistent_storage_writable ? "Persistent storage writable" : "Persistent storage not writable",
                 runtime->persistent_storage_writable ? "audit persistence path enabled" : "runtime remains restricted");

    map_status_to_claim(runtime->journal_txt_status, &state, &severity);
    append_claim(out_claims, max_claims, &count, "audit.journal_txt.available", state, severity,
                 "audit.journal", "/vita/audit/session-journal.txt",
                 "TXT session journal runtime status", "text audit journaling capability");

    map_status_to_claim(runtime->journal_jsonl_status, &state, &severity);
    append_claim(out_claims, max_claims, &count, "audit.journal_jsonl.available", state, severity,
                 "audit.journal", "/vita/audit/session-journal.jsonl",
                 "JSONL session journal runtime status", "JSONL audit stream capability");

    map_status_to_claim(runtime->transcript_txt_status, &state, &severity);
    append_claim(out_claims, max_claims, &count, "audit.transcript_txt.available", state, severity,
                 "audit.transcript", "/vita/audit/transcript.txt",
                 "TXT transcript runtime status", "text transcript export capability");

    map_status_to_claim(runtime->transcript_jsonl_status, &state, &severity);
    append_claim(out_claims, max_claims, &count, "audit.transcript_jsonl.available", state, severity,
                 "audit.transcript", "/vita/audit/transcript.jsonl",
                 "JSONL transcript runtime status", "JSONL transcript export capability");

    map_status_to_claim(runtime->sqlite_status, &state, &severity);
    append_claim(out_claims, max_claims, &count, "audit.sqlite.available", state, severity,
                 "audit.sqlite", "/vita/audit/audit.db",
                 "SQLite audit runtime status", "structured audit backend availability");

    append_claim(out_claims,
                 max_claims,
                 &count,
                 "operational.restricted",
                 runtime->restricted_mode ? VITA_TRI_YES : VITA_TRI_NO,
                 VITA_IR_SEVERITY_INFO,
                 "runtime.mode",
                 "operational",
                 runtime->restricted_mode ? "Restricted operational mode enabled" : "Restricted operational mode disabled",
                 runtime->restricted_mode ? "limited feature set active" : "full feature path permitted");

    append_claim(out_claims,
                 max_claims,
                 &count,
                 "operational.full",
                 runtime->restricted_mode ? VITA_TRI_NO : VITA_TRI_YES,
                 runtime->restricted_mode ? VITA_IR_SEVERITY_WARN : VITA_IR_SEVERITY_INFO,
                 "runtime.mode",
                 "operational",
                 runtime->restricted_mode ? "Full operational mode unavailable" : "Full operational mode enabled",
                 runtime->restricted_mode ? "remain in restricted diagnostics" : "full audited operation");

    return count;
}
