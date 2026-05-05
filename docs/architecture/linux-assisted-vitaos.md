# Linux-Assisted VitaOS architecture

Linux-Assisted VitaOS is a planned companion track for VitaOS. It does not replace Native VitaOS Core.

This document is architecture-only. It does not implement a Linux ISO, browser, network stack, remote AI bridge, countdown, boot handoff, scripts, dependencies, or native code changes.

## Purpose

The Linux-assisted track exists to make emergency usefulness practical sooner when Linux can provide mature hardware support.

Future goals:

- use Linux as a practical rescue/live base;
- provide browser support;
- provide network support through Linux drivers;
- provide accessibility tooling where Linux can help;
- allow remote AI assistance only when network/internet exists;
- mount, read, or copy `/vita` artifacts;
- read VitaIR-Tri claims from `/vita/export/reports/vitair-state.jsonl` when present;
- help the user in emergencies without inventing system state.

## Relationship to Native VitaOS Core

Native VitaOS Core remains the primary from-scratch, UEFI/live-first, text-first, audit-first system.

Linux-Assisted VitaOS is a future companion path for practical rescue use. Linux-specific assumptions must not leak into the native UEFI/freestanding core.

## Planned boot countdown behavior

Before starting the Linux-assisted environment, VitaOS should eventually show a short keyboard/text interrupt countdown.

This is planned behavior only. It is not implemented yet.

English flow:

```text
VitaOS Linux-Assisted boot
Press any key or type text to stay in Native VitaOS Core.
No input detected: starting Linux assistant in 3...
No input detected: starting Linux assistant in 2...
No input detected: starting Linux assistant in 1...
Starting Linux assistant...
```

Spanish flow:

```text
Arranque VitaOS Linux-Assisted
Presiona una tecla o escribe texto para quedarte en Native VitaOS Core.
Sin entrada detectada: iniciando asistente Linux en 3...
Sin entrada detectada: iniciando asistente Linux en 2...
Sin entrada detectada: iniciando asistente Linux en 1...
Iniciando asistente Linux...
```

## Keyboard/text cancellation semantics

Planned semantics:

- If keyboard input is detected during the countdown, auto-start is cancelled.
- If text input is detected during the countdown, auto-start is cancelled.
- After cancellation, VitaOS should stay in Native VitaOS Core, a boot selector, or a diagnostic console.
- If no input is detected after the 3-second countdown, VitaOS proceeds to the Linux-assisted path.
- The countdown is an interruptible default, not an irreversible forced path.

## Browser, network, and remote AI goals

Linux-assisted mode may eventually use Linux to provide browser, wired/Wi-Fi networking, accessibility tooling, and remote AI when internet exists.

Remote AI must consume observed evidence such as `/vita` artifacts, diagnostic reports, session reports, VitaIR-Tri claims, and explicit user context. It must not invent system state.

## Emergency mode goals

Future emergency goals:

- help read VitaOS diagnostics;
- help copy `/vita` reports to safe media;
- help open browser resources when network exists;
- summarize system state using VitaIR-Tri claims;
- distinguish known, unknown, unavailable, and planned capabilities.

## `/vita` artifact consumption

`/vita` is the continuity boundary between Native VitaOS Core and Linux-Assisted VitaOS.

Future Linux-assisted tooling should start read-only and prefer existing artifacts:

```text
/vita/audit/session-journal.txt
/vita/audit/session-journal.jsonl
/vita/audit/sessions/
/vita/export/reports/last-session.txt
/vita/export/reports/last-session.jsonl
/vita/export/reports/diagnostic-bundle.txt
/vita/export/reports/diagnostic-bundle.jsonl
/vita/export/reports/self-test.txt
/vita/export/reports/self-test.jsonl
/vita/export/reports/vitair-state.jsonl
```

## VitaIR-Tri consumption

Linux-assisted tools should treat VitaIR-Tri as evidence-bound input.

Rules:

- human output may show `+1`, `0`, `-1`;
- JSONL `state` must remain numeric: `1`, `0`, `-1`;
- JSONL must never emit `"state":+1`;
- `severity` is independent from `state`;
- unknown data must remain unknown.

## Audit and honesty rules

Do not claim these until implemented and validated:

- Linux-Assisted VitaOS ISO;
- Linux rescue image;
- 3-second countdown in code;
- browser integration;
- network/Wi-Fi in Native VitaOS Core;
- remote AI;
- AWS Bedrock integration;
- Hosted AI Bridge;
- Linux tooling that reads `/vita` or `vitair-state.jsonl`.

Do not store secrets in logs, reports, or claims. Do not bypass `/vita` artifacts when explaining system state.

## Risks

- confusing Linux-Assisted VitaOS with Native VitaOS Core;
- letting Linux assumptions leak into UEFI/freestanding code;
- claiming a Linux ISO before a real artifact exists;
- letting remote AI invent state;
- bypassing current audit/export conventions;
- overpromising emergency capability.

## Not implemented yet

This document does not implement:

- Linux-Assisted VitaOS ISO;
- Linux boot handoff;
- 3-second keyboard/text countdown;
- browser integration;
- network setup workflow;
- remote AI assistant;
- AWS Bedrock integration;
- Hosted AI Bridge;
- Linux tooling that reads `/vita`;
- Linux tooling that reads `vitair-state.jsonl`.

## Roadmap phases

1. L0: architecture only.
2. L1: read-only `/vita` and VitaIR-Tri artifact consumer.
3. L2: Linux live/rescue base research.
4. L3: countdown and selector design.
5. L4: remote AI bridge planning.
6. L5: real ISO/build artifact validation.
