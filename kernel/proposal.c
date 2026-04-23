/*
 * kernel/proposal.c
 * AI proposal MVP lifecycle.
 */

#include <vita/audit.h>
#include <vita/console.h>
#include <vita/proposal.h>

#ifdef VITA_HOSTED
#include <stdio.h>
#include <string.h>
#endif

#define MAX_PROPOSALS 8

typedef struct {
    char proposal_id[16];
    const char *proposal_type;
    const char *summary;
    const char *rationale;
    const char *risk_level;
    const char *benefit_level;
    bool requires_human_confirmation;
    const char *status;
} proposal_slot_t;

static proposal_slot_t g_proposals[MAX_PROPOSALS];
static int g_count = 0;

static void mem_zero(void *ptr, unsigned long n) {
    unsigned char *p = (unsigned char *)ptr;
    for (unsigned long i = 0; i < n; ++i) p[i] = 0;
}

static bool str_eq(const char *a, const char *b) {
    unsigned long i = 0;
    if (!a || !b) return false;
    while (a[i] && b[i]) {
        if (a[i] != b[i]) return false;
        i++;
    }
    return a[i] == b[i];
}

static bool starts_with(const char *s, const char *prefix) {
    unsigned long i = 0;
    if (!s || !prefix) return false;
    while (prefix[i]) {
        if (s[i] != prefix[i]) return false;
        i++;
    }
    return true;
}

static void id_from_index(int idx, char out[16]) {
    out[0] = 'P'; out[1] = '-';
    int n = idx + 1;
    if (n >= 10) {
        out[2] = (char)('0' + (n / 10));
        out[3] = (char)('0' + (n % 10));
        out[4] = '\0';
    } else {
        out[2] = (char)('0' + n);
        out[3] = '\0';
    }
}

void proposal_engine_reset(void) {
    mem_zero(g_proposals, sizeof(g_proposals));
    g_count = 0;
}

static void add_proposal(const char *type, const char *summary, const char *rationale, const char *risk, const char *benefit) {
    if (g_count >= MAX_PROPOSALS) return;
    proposal_slot_t *p = &g_proposals[g_count];
    id_from_index(g_count, p->proposal_id);
    p->proposal_type = type;
    p->summary = summary;
    p->rationale = rationale;
    p->risk_level = risk;
    p->benefit_level = benefit;
    p->requires_human_confirmation = true;
    p->status = "PENDING";

    vita_ai_proposal_t row = {
        .proposal_id = p->proposal_id,
        .proposal_type = p->proposal_type,
        .summary = p->summary,
        .rationale = p->rationale,
        .risk_level = p->risk_level,
        .benefit_level = p->benefit_level,
        .requires_human_confirmation = true,
        .status = p->status,
    };
    audit_persist_ai_proposal(&row);
    audit_emit_boot_event("AI_PROPOSAL_CREATED", p->proposal_id);
    g_count++;
}

void proposal_generate_initial(const vita_handoff_t *handoff, const vita_hw_snapshot_t *hw, bool audit_ready) {
    const char *mode = (handoff && handoff->firmware_type == VITA_FIRMWARE_HOSTED) ? "hosted" : "uefi";
    proposal_engine_reset();

    add_proposal(
        "review_hardware_status",
        "Review hardware status / Revisar estado de hardware",
        hw && hw->ram_bytes > 0 ? "Hardware snapshot available with real RAM/CPU data" : "Hardware snapshot partial; verify detection limits",
        "low",
        "high");

    add_proposal(
        "review_audit_status",
        "Review audit status / Revisar estado de auditoria",
        audit_ready ? "Persistent SQLite backend is active" : "Audit backend is not active",
        audit_ready ? "low" : "high",
        "high");

    add_proposal(
        "enter_guided_emergency_mode",
        "Enter guided emergency mode / Entrar en modo emergencia guiado",
        "Provides structured questions and immediate actions",
        "medium",
        "high");

    add_proposal(
        "inspect_cooperative_node_readiness",
        "Inspect cooperative node readiness / Revisar nodos cooperativos",
        str_eq(mode, "hosted") ? "Boot mode hosted: validate node readiness stubs" : "UEFI mode: verify cooperative stack prerequisites",
        "medium",
        "medium");

    audit_emit_boot_event("AI_PROPOSALS_READY", "initial proposals generated");
}

void proposal_show_all(void) {
    for (int i = 0; i < g_count; ++i) {
        proposal_slot_t *p = &g_proposals[i];
        console_write_line("--- Proposal / Propuesta ---");
        console_write_line(p->proposal_id);
        console_write_line("1. Resumen / Summary:");
        console_write_line(p->summary);
        console_write_line("2. Riesgos inmediatos / Immediate risks:");
        console_write_line(p->risk_level);
        console_write_line("3. Preguntas criticas / Critical questions:");
        console_write_line("Is human approval required?");
        console_write_line("4. Acciones inmediatas sugeridas / Suggested actions:");
        console_write_line("approve <id> | reject <id>");
        console_write_line("5. Recursos del sistema / System resources:");
        console_write_line(p->benefit_level);
        console_write_line("6. Propuesta del sistema / System proposal:");
        console_write_line(p->rationale);
        console_write_line("7. Estado de auditoria / Audit state:");
        console_write_line(p->status);
        audit_emit_boot_event("AI_PROPOSAL_SHOWN", p->proposal_id);
    }
}

static proposal_slot_t *find_by_id(const char *id) {
    for (int i = 0; i < g_count; ++i) {
        if (str_eq(g_proposals[i].proposal_id, id)) return &g_proposals[i];
    }
    return 0;
}

int proposal_count_all(void) {
    return g_count;
}

int proposal_count_pending(void) {
    int pending = 0;
    for (int i = 0; i < g_count; ++i) {
        if (str_eq(g_proposals[i].status, "PENDING")) {
            pending++;
        }
    }
    return pending;
}

bool proposal_handle_command(const char *line) {
    if (!line) return false;

    if (starts_with(line, "list") || starts_with(line, "proposals")) {
        proposal_show_all();
        return true;
    }

    if (starts_with(line, "approve ")) {
        const char *id = line + 8;
        proposal_slot_t *p = find_by_id(id);
        if (!p) {
            console_write_line("proposal id not found");
            return true;
        }
        p->status = "APPROVED";
        audit_persist_human_response(p->proposal_id, "approve");
        audit_emit_boot_event("AI_PROPOSAL_APPROVED", p->proposal_id);
        console_write_line("approved");
        return true;
    }

    if (starts_with(line, "reject ")) {
        const char *id = line + 7;
        proposal_slot_t *p = find_by_id(id);
        if (!p) {
            console_write_line("proposal id not found");
            return true;
        }
        p->status = "REJECTED";
        audit_persist_human_response(p->proposal_id, "reject");
        audit_emit_boot_event("AI_PROPOSAL_REJECTED", p->proposal_id);
        console_write_line("rejected");
        return true;
    }

#ifdef VITA_HOSTED
    if (starts_with(line, "help")) {
        console_write_line("commands: status | hw | audit | proposals | peers | emergency | helpme | approve <id> | reject <id> | shutdown");
        return true;
    }
#endif

    return false;
}


void proposal_hosted_repl(void) {
#ifdef VITA_HOSTED
    char line[128];
    console_write_line("AI console ready. type: list | approve <id> | reject <id> | exit");
    while (fgets(line, sizeof(line), stdin)) {
        size_t n = strlen(line);
        while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r')) line[--n] = '\0';
        if (str_eq(line, "exit")) {
            console_write_line("bye");
            break;
        }
        if (!proposal_handle_command(line)) {
            console_write_line("unknown command");
        }
    }
#else
    console_write_line("proposal repl unavailable");
#endif
}
