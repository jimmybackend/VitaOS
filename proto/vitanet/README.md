# VitaNet v0 — VitaOS con IA

VitaNet es el protocolo mínimo entre nodos VitaOS.

## Objetivo F1

- descubrir peers;
- publicar capacidades;
- mantener heartbeat;
- proponer vínculo;
- asignar tareas simples;
- replicar auditoría de forma básica.

## Mensajes base

- `HELLO`
- `CAPS`
- `HEARTBEAT`
- `LINK_ACCEPT`
- `LINK_REJECT`
- `AUDIT_BLOCK` (base mínima de replicación en hosted)

## Implementación inicial

Transporte implementado en este slice:
- Hosted UDP/IPv4 sobre Ethernet del host

Transporte prioritario objetivo:
- Ethernet

## Reglas para Codex

- no asumir cifrado completo en F1;
- no agregar discovery complejo fuera de alcance;
- registrar en auditoría descubrimiento, vínculo y tareas.

## Estructuras mínimas sugeridas

```c
typedef struct {
    char node_id[16];
    char arch[16];
    uint32_t ram_mb;
    uint32_t flags;
} vitanet_caps_t;

typedef struct {
    char peer_id[16];
    uint64_t first_seen_unix;
    uint64_t last_seen_unix;
    uint32_t transport_mask;
} vitanet_peer_t;
```
