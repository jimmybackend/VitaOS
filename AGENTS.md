# AGENTS.md — VitaOS con IA

This file provides working instructions for AI coding agents and contributors.

## Project identity

VitaOS with AI is a from-scratch operating system project focused on:
- live/RAM operation
- mandatory persistent audit
- kernel-resident AI proposal engine
- guided bilingual text console
- cooperative nodes
- emergency usefulness

## Current milestone

F1A:
- x86_64 UEFI boot
- basic text console
- hardware discovery (CPU, RAM, USB, storage, network)
- persistent audit using SQLite on writable USB partition
- initial AI proposal engine
- initial guided console

## Non-goals for the current milestone

Do not add:
- GUI
- STT/TTS
- full desktop features
- autonomous control of external physical systems
- broad hardware support claims without drivers
- unrelated userland features

## Coding rules

1. Prefer C for kernel code.
2. Use assembly only where required by boot/arch code.
3. Keep modules small and focused.
4. Preserve auditability in every important path.
5. Do not invent APIs without documenting them.
6. Prefer explicit structures over hidden side effects.
7. Keep code freestanding-friendly.
8. Explain assumptions briefly before major code generation.
9. If a feature affects behavior, define the audit events it emits.
10. Respect the SQLite schema as an authoritative contract.

## Audit-first rule

If a feature introduces:
- system state changes,
- AI proposals,
- human approvals/rejections,
- node discovery or linking,
- task assignment,
then it must be reflected in the audit model.

## Proposal engine behavior

AI output should use a structured operational format:
1. understood summary
2. immediate risks
3. critical questions
4. immediate suggested actions
5. available system resources
6. system proposal
7. audit state

## Repository guidance

Suggested root layout:
- kernel/
- arch/
- drivers/
- proto/
- knowledge/
- schema/
- docs/
- tools/
- tests/

## Working style

When asked to write code:
- identify target module;
- mention dependencies;
- produce complete code, not fragments, unless partial change was requested;
- avoid overengineering;
- keep milestone discipline.
