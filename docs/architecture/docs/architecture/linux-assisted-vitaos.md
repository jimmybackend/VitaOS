# Linux-Assisted VitaOS (Planned Companion Path)

## 1. Purpose

Linux-Assisted VitaOS is a **planned companion path** for scenarios where Native VitaOS Core may benefit from broader device support, browser access, richer network tooling, or remote AI connectivity.

Its purpose is to extend practical utility for emergency and assisted operations while preserving VitaOS identity, audit discipline, and explicit human control.

## 2. Relationship to Native VitaOS Core

Native VitaOS Core remains the primary and authoritative execution path.

Linux-Assisted VitaOS is planned as an optional companion environment that:
- starts only after explicit policy and boot conditions;
- does not redefine Native VitaOS contracts;
- consumes Native VitaOS outputs (`/vita`, VitaIR-Tri, audit artifacts) as inputs;
- can return control and artifacts back to Native VitaOS workflows.

## 3. What Linux-Assisted VitaOS is

Linux-Assisted VitaOS is planned as a bounded runtime layer that may provide:
- expanded hardware compatibility through Linux drivers;
- browser-capable workflows for guided operator tasks;
- stronger network interoperability for peer and remote services;
- controlled remote-AI assistance channels;
- emergency operation tooling when native capabilities are insufficient.

## 4. What it is not

Linux-Assisted VitaOS is not:
- a replacement for Native VitaOS Core;
- proof that full Linux boot integration is already implemented;
- proof of complete network protocol support;
- proof of complete remote AI autonomy;
- permission to make unverifiable persistence or audit claims.

## 5. Planned boot countdown behavior

Before starting the Linux-assisted environment, VitaOS should eventually show a short keyboard/text interrupt countdown.

Planned English flow:

```text
VitaOS Linux-Assisted boot
Press any key or type text to stay in Native VitaOS Core.
No input detected: starting Linux assistant in 3...
No input detected: starting Linux assistant in 2...
No input detected: starting Linux assistant in 1...
Starting Linux assistant...
```

This behavior is planned to keep Native VitaOS Core as the default human-interruptible path.

## 6. Keyboard/text cancellation semantics

Planned semantics:
- Any detected key press cancels Linux-assisted start.
- Any detected text input cancels Linux-assisted start.
- Cancellation keeps or returns execution to Native VitaOS Core guided console.
- Cancellation should be auditable as an operator intent event.

No claim is made here that this countdown/cancel path is already implemented.

## 7. Browser, network, and remote AI goals

Planned goals include:
- browser-mediated access for documentation, coordination, and guided recovery procedures;
- network-capable node cooperation beyond current minimal hosted validation;
- remote AI assistant integration under explicit policy gates;
- traceable boundaries between local authoritative state and remote advisory outputs.

All remote interactions must remain auditable and policy-constrained.

## 8. Emergency mode goals

Planned emergency-mode goals:
- maintain useful operator workflows when Native VitaOS path is constrained;
- provide communication and information retrieval support;
- keep a clear chain of custody for actions taken under degraded conditions;
- avoid silent escalation from advisory to autonomous control.

## 9. How it consumes `/vita`

Planned consumption model:
- `/vita` remains the canonical shared state surface;
- Linux-assisted components read structured artifacts from `/vita`;
- writes back into `/vita` must follow explicit contracts and verification rules;
- no component should claim successful persistence without equivalent read-back verification.

## 10. How it consumes VitaIR-Tri

Planned VitaIR-Tri consumption:
- use VitaIR-Tri as a structured proposal and reasoning input;
- preserve claim discipline (no invented capabilities, no secret persistence, no ANSI escape content in claims);
- preserve provenance from proposal source to operator-visible output.

Linux-assisted usage must not reinterpret VitaIR-Tri as autonomous authority.

## 11. Audit and honesty rules

Linux-Assisted VitaOS must follow the same audit-first and honesty discipline:
- state changes, proposals, approvals/rejections, link intent, and task intent should be auditable or explicitly deferred;
- capability claims must match what is implemented;
- success claims (storage, network, replication, AI) must be verifiable;
- uncertain or degraded conditions must be reported explicitly.

## 12. Risks

Key planned risks include:
- operator confusion between planned vs implemented capabilities;
- dependency growth that dilutes milestone focus;
- trust boundary mistakes between local core and remote services;
- audit gaps across environment handoff points;
- security expansion due to browser/network surface area.

## 13. Not implemented yet

This document describes a **planned** architecture direction.

As of this milestone, Linux-Assisted VitaOS boot path, countdown interrupt flow, and full integration contracts are not declared implemented by this document.

## 14. Roadmap phases

Planned phased approach:

1. **Definition phase**
   - finalize scope boundaries with Native VitaOS Core;
   - define auditable handoff contract and `/vita` interfaces.
2. **Boot-interrupt phase**
   - implement minimal countdown and keyboard/text cancellation path;
   - emit audit events for timer and cancellation outcomes.
3. **Companion runtime phase**
   - launch constrained Linux-assisted runtime with explicit policy gates;
   - enable minimal browser/network assistance flows.
4. **Remote-assist phase**
   - add policy-bounded remote AI connectors with provenance tracking;
   - ensure operator approval points are explicit.
5. **Hardening phase**
   - validate audit continuity, failure handling, and rollback behaviors;
   - tighten claim discipline and emergency-mode operator guidance.
