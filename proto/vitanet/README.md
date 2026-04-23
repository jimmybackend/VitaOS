# VitaNet v0 — VitaOS con IA

VitaNet es el protocolo mínimo entre nodos VitaOS.

## Objetivo F1B inicial

- descubrir peers;
- publicar capacidades;
- mantener heartbeat;
- proponer vínculo;
- intercambiar tareas mínimas (`node_task`);
- replicar auditoría de forma básica.

## Mensajes base

- `HELLO`
- `CAPS`
- `HEARTBEAT`
- `LINK_ACCEPT`
- `LINK_REJECT`
- `TASK`
- `TASK_ACK`
- `TASK_REJECT`
- `TASK_DONE`
- `AUDIT_BLOCK`

## Implementación inicial

Transporte implementado en este slice:
- Hosted UDP/IPv4 sobre Ethernet del host

Transporte prioritario objetivo:
- Ethernet

## Reglas para Codex

- no asumir cifrado completo en F1;
- no agregar discovery complejo fuera de alcance;
- registrar en auditoría descubrimiento, vínculo, tareas y replicación.
