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
- `PROPOSAL`
- `LINK_ACCEPT`
- `LINK_REJECT`
- `TASK_ASSIGN`
- `TASK_DONE`
- `AUDIT_REPL`

## Implementación inicial

Transportes prioritarios:
- Ethernet
- Wi-Fi

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
