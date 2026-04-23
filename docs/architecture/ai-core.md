# AI Core — MVP F1A

## Enfoque

Motor residente simple (sin LLM pesado):

1. Percepción: usa estado real (`audit_ready`, `hardware_snapshot`, modo boot).
2. Evaluación: clasifica riesgo/beneficio y necesidad de confirmación humana.
3. Propuesta estructurada: genera propuestas bilingües en formato operativo.
4. Persistencia: guarda en `ai_proposal`.
5. Presentación: imprime propuestas en consola.
6. Respuesta humana: `approve <id>` / `reject <id>`.
7. Auditoría: emite eventos y persiste `human_response`.

## Tipos iniciales

- `review_hardware_status`
- `review_audit_status`
- `enter_guided_emergency_mode`
- `inspect_cooperative_node_readiness`

## Eventos de auditoría

- `AI_PROPOSAL_CREATED`
- `AI_PROPOSAL_SHOWN`
- `AI_PROPOSAL_APPROVED`
- `AI_PROPOSAL_REJECTED`

## Comandos de consola guiada (hosted)

- `status`
- `hw`
- `audit`
- `proposals` (o `list`)
- `peers`
- `helpme`
- `emergency <texto libre>`
- `approve <id>`
- `reject <id>`
- `shutdown`

## Smoke test

```bash
make smoke-audit
```

Valida que existan filas en `ai_proposal` y `human_response`, además del rastro en `audit_event`.
