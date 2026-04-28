# AGENTS.md — VitaOS con IA

This file provides working instructions for AI coding agents and contributors.

## Project identity

VitaOS with AI is a from-scratch operating system project focused on:

- live/RAM operation
- mandatory persistent audit
- kernel-resident proposal engine
- guided bilingual text console
- cooperative nodes
- emergency usefulness
- milestone discipline over feature breadth

## Current practical milestone

The repository is currently centered on a real minimal F1A/F1B slice.

### F1A integrated base
- x86_64 UEFI boot
- early text console
- `kmain()` integration
- basic hardware discovery
- guided console minimum
- visible proposals
- audit model aligned with SQLite schema

### F1B minimum real
- hosted VitaNet minimum
- peer discovery
- `node_peer` persistence
- initial link proposal
- recent audit block export
- base contract for `node_task`

## Important current reality

### UEFI path
UEFI currently validates boot, console, banner and restricted diagnostic flow.

Do not assume a complete persistent audit backend already exists in the freestanding UEFI path.

### Hosted path
Hosted is currently the main validation path for:
- persistent SQLite audit
- `boot_session`
- `hardware_snapshot`
- `audit_event`
- `ai_proposal`
- `human_response`
- `node_peer`
- basic VitaNet replication tests

## Non-goals for the current milestone

Do not add:
- GUI
- STT/TTS
- broad userland
- advanced scheduler work
- full network protocol stack
- complete bidirectional replication claims
- autonomous control features
- unrelated desktop features
- speculative APIs not needed by the current slice

## Coding rules

1. Prefer C for kernel code.
2. Use assembly only where boot or arch code requires it.
3. Keep modules small, explicit and milestone-focused.
4. Preserve auditability in every important path.
5. Do not invent APIs without an immediate repo-level need.
6. Prefer explicit structures over hidden behavior.
7. Keep code freestanding-friendly.
8. Respect the SQLite schema as an authoritative contract.
9. If behavior changes, verify audit impact.
10. Return full copy-pasteable files when requested.

## Audit-first rule

If a feature introduces:
- system state changes
- AI proposals
- human approvals or rejections
- peer discovery
- link state changes
- task assignment intent
- audit replication intent

then it must be reflected in the audit model or be explicitly deferred.

## Repository discipline

Before changing code:

1. identify the exact target file;
2. check which already-existing structs or functions it depends on;
3. avoid assuming fields, tables or handoff members that do not exist;
4. keep compatibility with the current repo layout;
5. do not silently widen scope.

## Proposal engine behavior

Operational output should stay structured around:

1. understood summary
2. immediate risks
3. critical questions
4. immediate suggested actions
5. available system resources
6. system proposal
7. audit state

## Console behavior

The console is currently text-first and guided.

Keep it:
- simple
- bilingual where already present
- compatible with hosted and UEFI paths
- useful for the current slice
- free of shell-framework overengineering

## Network / node behavior

For the current milestone:
- prefer hosted validation
- keep VitaNet minimal
- do not claim full protocol completeness
- keep peer state explicit
- keep replication basic and auditable

## Working style

When asked to write code:
- identify the exact target module;
- mention dependencies when they matter;
- produce complete files, not fragments, unless partial edits were requested;
- avoid overengineering;
- keep the repo coherent with the current slice.

## Priority files for this slice

- `Makefile`
- `include/vita/boot.h`
- `include/vita/console.h`
- `include/vita/hw.h`
- `include/vita/audit.h`
- `include/vita/node.h`
- `include/vita/proposal.h`
- `arch/x86_64/boot/uefi_entry.c`
- `arch/x86_64/boot/host_entry.c`
- `kernel/kmain.c`
- `kernel/console.c`
- `kernel/hardware_discovery.c`
- `kernel/proposal.c`
- `kernel/node_core.c`
- `kernel/audit_core.c`
- `kernel/panic.c`
- `schema/audit.sql`
- `tools/test/smoke-boot.sh`
- `tools/test/smoke-audit.sh`

## Final instruction for AI agents

Do not optimize for impressive scope.

Optimize for:
- build coherence
- truthful behavior
- audit consistency
- milestone closure
- complete files ready to paste

## Boot storage bootstrap rule (current)

- `/vita` must be prepared automatically during boot (hosted + UEFI) without requiring manual `storage repair`.
- Do not report `storage verified`, `journal active`, `saved`, `written` or `audit ready` style claims unless write->read->compare (or equivalent verified read-back) actually succeeded.
- For UEFI/Rufus flows, prefer selecting a writable Simple File System candidate; if no writable candidate is verifiable, fail honestly (restricted diagnostic).
