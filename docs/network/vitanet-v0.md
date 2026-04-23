# VitaNet v0 — Slice F1B inicial

## Alcance real implementado

Este slice implementa **solo en hosted** una base mínima verificable de VitaNet sobre UDP/IPv4 (Ethernet del host):

1. inicio de VitaNet;
2. handshake con peer de prueba;
3. descubrimiento y persistencia en `node_peer`;
4. propuesta de enlace (`link_with_peer`) con aprobación/rechazo humano;
5. envío mínimo de bloque de auditoría reciente (`AUDIT_BLOCK ...`) como base de replicación.

## Mensajes v0 usados en este slice

- `HELLO <node_id>`
- `CAPS <json>`
- `HEARTBEAT <payload>`
- `LINK_ACCEPT <node_id>`
- `LINK_REJECT <node_id>`

## Flujo

1. `node_core_start()` emite `VITANET_STARTED`.
2. Envía `HELLO` y `CAPS` al endpoint seed (`vitanet_seed_endpoint`).
3. Recibe `HELLO/CAPS/HEARTBEAT` del peer de prueba.
4. Persiste peer en `node_peer` vía `audit_upsert_node_peer()`.
5. Emite propuesta IA `N-1` (`link_with_peer`) y evento `LINK_PROPOSAL_CREATED`.
6. En consola, operador ejecuta `approve N-1` o `reject N-1`.
7. Se actualiza `link_state` en `node_peer` y se envía `LINK_ACCEPT`/`LINK_REJECT`.
8. Se exporta bloque corto de auditoría reciente y se envía como `AUDIT_BLOCK ...`, con evento `AUDIT_REPLICATION_ATTEMPTED`.

## Eventos auditados

- `VITANET_STARTED`
- `PEER_DISCOVERED`
- `PEER_CAPABILITIES_RECEIVED`
- `LINK_PROPOSAL_CREATED`
- `LINK_ACCEPTED` / `LINK_REJECTED`
- `HEARTBEAT_RECEIVED`
- `AUDIT_REPLICATION_ATTEMPTED`

## Limitaciones explícitas

- UEFI freestanding aún no incluye networking completo en F1B inicial.
- El peer real en pruebas se provee con harness (`tools/test/vitanet-peer.py`).
- No hay cifrado/consenso ni replicación distribuida avanzada en este slice.

## Base para `node_task`

Se anuncia capacidad mínima `"node_task_v0": true` en `CAPS` para preparar la evolución hacia `node_task`.
La ejecución de tareas distribuidas queda fuera de este slice.
