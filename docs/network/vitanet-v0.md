# VitaNet v0 — Slice F1B inicial

## Alcance real implementado

Este slice implementa en hosted una base mínima verificable de VitaNet sobre UDP/IPv4 (Ethernet del host):

1. inicio de VitaNet;
2. handshake con peer;
3. descubrimiento y persistencia en `node_peer`;
4. propuesta de enlace (`N-1`) con approve/reject;
5. tareas entre nodos (`node_task`) con persistencia;
6. replicación básica bidireccional de auditoría por bloques mínimos.

## Mensajes v0 usados

- `HELLO <node_id>`
- `CAPS <json>`
- `HEARTBEAT <payload>`
- `LINK_ACCEPT <node_id>`
- `LINK_REJECT <node_id>`
- `TASK <type>|<task_id>|<payload_json>`
- `TASK_ACK <task_id>`
- `TASK_REJECT <task_id>`
- `TASK_DONE <task_id>`
- `AUDIT_BLOCK <event_seq:event_type:event_hash;...>`

## `node_task` mínimo operativo

Estados soportados en este slice:

- `created`
- `sent`
- `accepted`
- `rejected`
- `done`
- `failed`

Tipos iniciales:

- `peer_status_request`
- `audit_replicate_range`

Payload JSON mínimo:

- `peer_status_request`: `{"request":"peer_status"}`
- `audit_replicate_range`: `{"range":"recent","limit":3}`

## Flujo mínimo implementado

1. A descubre/acepta enlace con B.
2. A crea `peer_status_request` (`node_task`) y la envía.
3. B acepta y responde (`TASK_DONE`).
4. A crea `audit_replicate_range` y la envía.
5. B acepta, exporta bloque y responde `AUDIT_BLOCK`.
6. A importa referencias deduplicadas y persiste `AUDIT_REPL_IMPORTED`.
7. A también envía su `AUDIT_BLOCK` para ida/vuelta mínima.

## Eventos auditados

- `NODE_TASK_CREATED`
- `NODE_TASK_SENT`
- `NODE_TASK_ACCEPTED`
- `NODE_TASK_REJECTED`
- `NODE_TASK_COMPLETED`
- `AUDIT_REPLICATION_STARTED`
- `AUDIT_REPLICATION_RECEIVED`
- `AUDIT_REPLICATION_PERSISTED`
- `AUDIT_REPLICATION_FAILED`
- `PEER_STATUS_REQUESTED`
- `PEER_STATUS_RECEIVED`

## Límites explícitos

- No hay consenso distribuido ni cifrado complejo.
- La réplica remota se importa como referencias (`AUDIT_REPL_IMPORTED`), no como cadena remota completa idéntica.
- UEFI freestanding mantiene límites de networking en este milestone.
