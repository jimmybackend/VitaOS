# VitaOS Implemented vs Planned Matrix (post PR #53 / #54)

This document clarifies what VitaOS currently does, what is partial (especially hosted-first), what is planned, and what must **not** be claimed yet.

## Classification legend

- **Implemented now**: available and usable in current repository behavior.
- **Partially implemented / hosted-first**: present in hosted path and/or incomplete across hosted + UEFI.
- **Planned / future**: intended direction, not implemented as milestone-complete behavior.
- **Must not be claimed yet**: claims that are explicitly disallowed unless validated by real implementation.

## Implemented vs planned matrix

| Area | Status | Notes |
|---|---|---|
| UEFI boot | **Implemented now** | UEFI boot path exists with boot/banner/console-oriented flow and restricted diagnostics. |
| hosted build | **Implemented now** | Hosted path is the primary validation path for current milestone behavior. |
| `/vita` persistent tree | **Implemented now** | Must be auto-prepared during boot (hosted + UEFI) without manual `storage repair`. |
| storage verification | **Partially implemented / hosted-first** | Any `verified/ready/written` style message must only be emitted after real write→read verification. Coverage is not to be over-claimed in all paths. |
| audit TXT/JSONL journal | **Implemented now** | Text/JSONL audit journal flows are part of the current slice expectations and should remain format-stable. |
| SQLite hosted vs SQLite UEFI | **Partially implemented / hosted-first** | SQLite-backed persistent audit is a hosted reality; full freestanding UEFI SQLite persistence must not be assumed. |
| session rotation | **Implemented now** | Current rotation behavior exists; treat as stable contract for this milestone. |
| last-session export | **Implemented now** | Current export behavior exists; treat as stable contract for this milestone. |
| diagnostic bundle | **Implemented now** | Diagnostic bundle support exists in current operational tooling flow. |
| selftest | **Implemented now** | Selftest capability exists in current command/tooling surface. |
| safe editor | **Implemented now** | Safe text editing workflow exists in current console-oriented path. |
| VitaIR-Tri claims | **Implemented now (with strict constraints)** | Claims discipline is mandatory: no invented capability, no secrets, no ANSI escapes, no false autonomy/network/storage assertions. |
| export `vitair` / `vitair-state.jsonl` | **Implemented now** | Export surfaces exist; JSONL shape must remain stable unless explicitly versioned and migrated. |
| network / Wi‑Fi | **Planned / future** | Only minimal hosted VitaNet peer/discovery slice should be assumed; full Wi‑Fi/network stack is not complete. |
| AWS Bedrock | **Planned / future** | Not milestone-complete as a validated always-available runtime capability. |
| local AI | **Planned / future** | Full local autonomous AI runtime is not implemented as a complete guaranteed capability. |
| Hosted AI Bridge | **Planned / future** | No production Hosted AI Bridge should be claimed yet. Any hosted integration surface must remain explicitly experimental or planned until implemented and validated. |
| GUI / browser | **Planned / future** | Explicit non-goal for current milestone. |
| Linux-Assisted VitaOS ISO | **Planned / future** | No Linux-Assisted VitaOS ISO should be claimed yet. This track is architectural/planned until a real build path, artifact, and validation flow exist. |
| ISO/USB real validation | **Planned / future** | Real hardware validation matrix is not complete enough for broad claims. |

## Must not be claimed yet (explicit)

The following claims are out-of-scope unless newly implemented and validated:

1. Full UEFI-side SQLite persistent audit parity with hosted mode.
2. Fully complete bidirectional network replication protocol.
3. Complete Wi‑Fi/network stack coverage across hardware.
4. Full AWS Bedrock operational guarantee in all deployment modes.
5. Full local AI autonomy with complete safety/ops controls.
6. Production-complete GUI/browser userland.
7. Broad ISO/USB hardware validation closure across diverse machines.
8. Production Hosted AI Bridge.
9. Linux-Assisted VitaOS ISO or rescue image.

## Claiming policy reminder

When reporting status externally (docs, console text, proposals, audit summaries):

- Prefer precise wording: `implemented`, `hosted-first`, `planned`, `not yet claimable`.
- Do not upgrade a capability from planned/partial to implemented without matching code + validation evidence.
- Keep VitaIR-Tri and audit-related statements aligned with actual verified behavior.
