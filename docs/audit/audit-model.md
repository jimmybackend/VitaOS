# Audit Model — VitaOS con IA

## Principio

Sin auditoría persistente válida, VitaOS no entra en modo operativo completo.

## Fuente de verdad

`schema/audit.sql` es el contrato canónico del modelo de auditoría.

## Objetos principales

- `boot_session`
- `hardware_snapshot`
- `audit_event`
- `ai_proposal`
- `human_response`
- `node_peer`
- `node_task`
- `knowledge_pack`

## Reglas

1. Todo cambio importante debe emitir al menos un `audit_event`.
2. Toda propuesta de IA debe reflejarse en `ai_proposal`.
3. Toda respuesta humana debe reflejarse en `human_response`.
4. Todo peer descubierto debe persistirse en `node_peer`.
5. Toda tarea entre nodos debe persistirse en `node_task`.
6. Toda carga de knowledge pack debe persistirse en `knowledge_pack`.

## Integridad

Cada evento debe calcular `event_hash` y enlazarse al `prev_hash`.

## Exportación

F1 puede exportar:
- SQL dump
- JSONL
- snapshot resumido para análisis externo

## Decisión de diseño

Aunque SQLite es el backend, el modelo sigue siendo append-mostly.
