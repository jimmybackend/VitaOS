# Linux-Assisted VitaOS Boot Countdown (Planned Design)

## 1. Purpose

Define a **small, explicit, and auditable** design for a 3-second countdown that runs before launching the Linux assistant path, allowing the operator to interrupt and stay in a Native VitaOS path.

This document is design-only. It intentionally does **not** add implementation.

## 2. Relationship to `docs/architecture/linux-assisted-vitaos.md`

`linux-assisted-vitaos.md` describes the broader Linux-assisted architecture strategy, boundaries, and expected responsibilities.

This document narrows scope to one specific interaction point in boot:

- the pre-handoff countdown window,
- the input interruption decision,
- fallback routing targets,
- and audit expectations for truthful boot claims.

In short:

- `linux-assisted-vitaos.md` = high-level architecture track
- `linux-assisted-boot-countdown.md` = detailed planned boot UX/control behavior

## 3. Planned countdown UX

At boot entry (hosted and/or Linux-assisted-capable path), the operator sees a short text-first prompt before assistant handoff.

Required visible flow (English baseline):

```text
VitaOS Linux-Assisted boot
Press any key or type text to stay in Native VitaOS Core.
No input detected: starting Linux assistant in 3...
No input detected: starting Linux assistant in 2...
No input detected: starting Linux assistant in 1...
Starting Linux assistant...
```

Design intent:

- Keep wording direct and operational.
- Keep the sequence deterministic (3 → 2 → 1).
- Keep this interaction available in text console style, compatible with current milestone discipline.
- Avoid decorative output that obscures operational truth.

## 4. Keyboard/text interruption semantics

Interruption is triggered by any operator input observed during the countdown window, including:

- single key press,
- newline/enter,
- short typed text.

Planned semantics:

- First detected input is enough to interrupt automatic Linux assistant launch.
- Input is interpreted as operator intent to remain in a Native VitaOS-controlled path.
- No requirement for full line editing during this minimal window.
- The countdown should stop immediately once input is detected.

## 5. Default behavior when no input is detected

If the full countdown completes with no detectable input:

1. Emit the final line: `Starting Linux assistant...`
2. Transition to the Linux assistant target path.
3. Record a truthful audit event that no input was observed during the decision window.

This default keeps Linux-assisted behavior automatic while preserving a clear manual override.

## 6. Behavior when input is detected

If input is detected at any point before countdown expiration:

1. Cancel Linux assistant auto-launch for that boot attempt.
2. Route to configured Native VitaOS fallback target (see Section 7).
3. Record audit events indicating:
   - countdown was active,
   - input was detected,
   - selected fallback target.

Important: input detection means “operator requested Native VitaOS path,” not “parse complex command mode.”

## 7. Fallback target options

Planned fallback target is configurable by policy/build/runtime mode.

### A) Native VitaOS Core

- Continue directly into Native VitaOS Core startup path.
- Best for normal operator override when core boot is expected to proceed.

### B) boot selector

- Enter a minimal text boot selector.
- Allows explicit choice among available targets (e.g., Native Core, Linux assistant, diagnostics).
- Best when multiple valid boot intents exist and human confirmation is preferred.

### C) diagnostic console

- Enter restricted diagnostic console path.
- Best for troubleshooting and incident response.
- Must remain truthful about what is and is not available in the current environment.

## 8. Audit expectations

Audit-first expectations for this flow:

- Countdown start should be auditable.
- Each meaningful decision state should be auditable (no-input timeout vs. input-interrupt).
- Final chosen boot target should be auditable.
- Audit entries must avoid exaggerated claims about persistence or subsystem readiness.

Minimum conceptual audit event set (names illustrative, not API commitments):

- `boot_countdown_started`
- `boot_countdown_no_input_timeout` **or** `boot_countdown_input_detected`
- `boot_target_selected`

## 9. Possible future VitaIR-Tri claims

If/when implemented, VitaIR-Tri output may claim only validated truths such as:

- countdown offered,
- operator input detected or not detected,
- selected target path,
- transition result status.

Claims must not imply capabilities not implemented (e.g., full autonomous orchestration, full network guarantees, or unsupported persistent backend behavior).

## 10. Safety and honesty rules

This design follows strict honesty constraints:

- Do not claim Linux assistant started unless handoff was actually initiated.
- Do not claim Native Core continuation unless routing actually occurred.
- Do not emit persistence/journal/write-success language without verified read-back semantics where required.
- Keep console text free of ANSI-dependent hidden signaling in critical status lines.

## 11. What is not implemented yet

Not implemented by this document:

- countdown timer integration in boot code,
- keyboard polling loop wiring for this decision window,
- fallback selector implementation details,
- concrete audit schema/event insertion changes,
- UEFI/hosted parity implementation and tests for this path.

This is architecture guidance only.

## 12. Future implementation checklist for a small C PR

A minimal future C PR should stay narrow and auditable:

1. Add a small boot-countdown state routine in the relevant boot path module.
2. Print the defined operator-facing lines (or approved bilingual equivalent) exactly once per step.
3. Poll/check keyboard/text input with immediate interrupt behavior.
4. Route to one configured fallback target when interrupted.
5. Route to Linux assistant target when countdown expires without input.
6. Emit audit events for start, decision, and selected target.
7. Add/update smoke tests for:
   - no-input path,
   - input-interrupt path,
   - truthful output expectations.
8. Document any final wording differences in architecture docs.

Scope guard for that PR:

- Do not widen into full boot menu framework work.
- Do not add unrelated networking or AI orchestration behavior.
- Keep milestone focus: small boot decision window with truthful audit.
