# VitaOS Architecture Tracks (Post-PR #52)

## Purpose

This document defines the architectural split used to guide near-term and mid-term VitaOS planning after PR #52 (merged on May 5, 2026).

The goal is clarity, not scope expansion:

- preserve truthful claims,
- preserve audit-first behavior,
- preserve emergency usefulness,
- avoid coupling native core decisions to unimplemented Linux assumptions.

## Why this split exists

VitaOS currently has two valid but different needs:

1. A **native, from-scratch operating-system core** that boots independently and remains useful in constrained/offline emergency contexts.
2. A **future Linux-assisted companion path** that can accelerate practical capabilities (drivers, network tooling, browser, accessibility, remote AI usage) when available.

Without an explicit split, design discussions can accidentally blur implemented behavior with aspirational behavior. This document prevents that drift.

## Current real implemented state

As of PR #52:

- VitaOS is centered on an audit-first, text-first, live/UEFI-first architecture.
- `/vita` persistent storage behavior is a core operational concept.
- JSONL-style reporting artifacts and session history flows are part of the practical workflow.
- Safe editor and export-oriented flows are part of the user-facing operational model.
- VitaIR-Tri runtime claims exist and must remain disciplined/truthful.
- PR #52 only cleaned up VitaIR-Tri formatting helpers in `kernel/command_core.c`; visible formats were not changed.

## Explicit not-yet-implemented capabilities

The project must **not** claim any of the following unless implemented and validated:

- full real network operation,
- AWS Bedrock integration,
- full local AI autonomy,
- Hosted AI Bridge completion,
- complete SQLite persistence in freestanding UEFI path,
- GUI/browser stack in native core,
- published Linux-based VitaOS ISO.

## Track 1: Native VitaOS Core

### Definition

Native VitaOS Core is the from-scratch VitaOS line.

### Principles

- UEFI/live-first boot model.
- Text-first operational interface.
- Audit-first behavior and evidence discipline.
- Emergency/offline capability as a primary goal.
- Must not depend on Linux in order to boot.

### Core operational artifacts

Native core uses and maintains:

- `/vita` persistent tree,
- JSONL reports,
- session history,
- export flows,
- storage diagnostics,
- VitaIR-Tri claims.

### Integrity constraints

Native core claims must remain conservative and implementation-bound.
It must not claim network/AWS/full local AI/Hosted AI Bridge/full UEFI SQLite persistence unless those capabilities are actually present and validated.

## Track 2: Linux-Assisted VitaOS

### Definition

Linux-Assisted VitaOS is a **future** practical companion environment that may use Linux capabilities to extend usefulness in connectivity-rich or hardware-diverse contexts.

### Principles

- Complements Native VitaOS Core; does not replace it.
- May leverage Linux drivers, networking, browser tooling, accessibility stacks, and remote AI paths when internet exists.
- Must consume `/vita` artifacts and VitaIR-Tri claims as system truth inputs.
- Must not invent core system state separate from audited/native outputs.
- Must not push Linux-specific assumptions into the native UEFI core.

### Current boundary

No claim is made that a Linux VitaOS ISO exists today.

## Relationship with `/vita`

`/vita` is the continuity boundary between tracks.

- Native core is the authoritative producer of core boot/session/audit artifacts for native operation.
- Linux-assisted tools (when implemented) should read, validate, and extend workflows from existing `/vita` data.
- Cross-track tooling should prefer compatibility with established `/vita` structure over ad-hoc parallel state stores.

## Relationship with VitaIR-Tri

VitaIR-Tri is the runtime claim layer for operational understanding and recommendations.

Across both tracks:

- claims must stay auditable and evidence-bound,
- claims must avoid unimplemented capability assertions,
- claims must not contain secret persistence,
- claims should remain machine-consumable and human-clear.

Linux-assisted layers must treat native VitaIR-Tri outputs as inputs to interpret, not overwrite with invented platform assumptions.

## Risks

1. **Capability drift**: wording that implies implemented network/AI/hosted capabilities when only stubs or plans exist.
2. **Boundary erosion**: Linux convenience decisions leaking into native UEFI assumptions.
3. **Audit dilution**: adding auxiliary flows that bypass `/vita` and reduce traceability.
4. **Safety overclaim**: emergency users receiving confidence not supported by actual system capability.

## Recommended next PRs

1. Add cross-reference links in existing docs to this architecture split.
2. Add a concise “implemented vs planned” matrix for Native core and Linux-assisted track.
3. Add a VitaIR-Tri claim taxonomy note that maps each claim category to evidence source.
4. Add `/vita` artifact contract notes for future Linux-assisted consumers (read-only first).

## Testing policy

### Docs-only PRs

- Run documentation diff/review checks only.
- Do **not** run `make`, validators, or `make iso` by default.

### Small C PRs

- Run `make hosted`.
- Run `make`.
- Run focused manual test for the changed flow.

### ISO/USB phase

- Run full validation only when explicitly requested for that phase.

## Scope guardrails for this document

This document is descriptive and architectural only.
It does **not** introduce code, schema, storage, or format changes.
