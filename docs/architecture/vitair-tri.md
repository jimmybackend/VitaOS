# VitaIR-Tri

VitaIR-Tri is VitaOS Internal Representation for auditable ternary claims.

It is **not** BitNet, not a neural-network format, not a language model, and not unary arithmetic. It is a small, text-first internal representation used to describe system state, diagnostics, audit status and future AI reasoning inputs.

## Core definition

VitaIR-Tri expresses a claim as a structured record with:

- a stable claim name,
- a ternary state,
- an independent severity,
- minimal context fields (`actor`, `target`, `meaning`, `effect`),
- and audit-safe text values.

The representation is intended to be human-auditable first, machine-consumable second.

## Ternary state

| State | Meaning |
|---:|---|
| `+1` | yes / true / available / accepted / OK |
| `0` | unknown / pending / neutral / not evaluated |
| `-1` | no / false / unavailable / rejected / failed |

## Severity is separate

A negative state does **not** always mean a critical failure.

Allowed severity levels:

- `info`
- `warn`
- `error`
- `critical`

Example:

```json
{
  "ir_version": "vitair-tri/0.1",
  "claim": "audit.sqlite.available",
  "state": -1,
  "severity": "warn",
  "actor": "audit.sqlite",
  "target": "/vita/audit/audit.db",
  "meaning": "SQLite audit is unavailable",
  "effect": "restricted diagnostic"
}
```

This means SQLite is unavailable, while other subsystems (such as storage or JSONL journal) may still be operational.

## Future JSONL-oriented examples

VitaIR-Tri is designed to be exported as JSONL in future phases.

```json
{"ir_version":"vitair-tri/0.1","claim":"storage.writable","state":+1,"severity":"info","actor":"storage","target":"/vita","meaning":"persistent storage writable","effect":"normal"}
{"ir_version":"vitair-tri/0.1","claim":"journal.jsonl.active","state":+1,"severity":"info","actor":"audit.journal","target":"/vita/audit/session-journal.jsonl","meaning":"JSONL journal active","effect":"audit stream available"}
{"ir_version":"vitair-tri/0.1","claim":"audit.sqlite.available","state":-1,"severity":"warn","actor":"audit.sqlite","target":"/vita/audit/audit.db","meaning":"SQLite backend unavailable in this path","effect":"hosted/UEFI feature split"}
```

## Relationship with TXT/JSONL audit journal

- VitaIR-Tri is an internal claim representation model.
- Existing TXT/JSONL audit journal remains authoritative for event transcripts.
- VitaIR-Tri claims should be mappable into audit entries without replacing current journal outputs.

## Relationship with SQLite audit

- VitaIR-Tri does **not** replace SQLite.
- SQLite schema and tables remain authoritative when SQLite backend is available.
- VitaIR-Tri can coexist with SQLite by describing availability and state claims, including partial-path limitations.

## Current limits (this milestone)

- Concept/documentation phase only.
- No kernel behavior changes.
- No storage behavior changes.
- No schema changes.
- No export contract changes.
- No selftest behavior changes.
- No hosted AI bridge implementation.
- No network/AWS claims added.

## Safety rules for claims

VitaIR-Tri claims must:

1. follow audit-first discipline;
2. avoid invented capabilities;
3. avoid persistence of secrets;
4. avoid ANSI escape sequences in persisted values;
5. avoid claiming unimplemented features (for example full UEFI SQLite persistence, full network/AWS integration, or complete local AI autonomy);
6. preserve truthful, minimally scoped meaning.
