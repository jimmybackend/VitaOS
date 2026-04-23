# Audit Replication — F1B initial

## Modelo elegido en este slice

En F1B inicial **no** se replica la cadena remota dentro de la cadena local como eventos remotos idénticos.
En su lugar, cada nodo importa referencias de eventos remotos como `AUDIT_REPL_IMPORTED`.

- campo `summary`: `remote_event_hash`
- campo `details_json`: `origin_peer`, `remote_boot_id`, `remote_event_seq`, `remote_event_type`

## Criterio anti-duplicados

Antes de persistir una importación, se consulta si ya existe:

- `event_type = 'AUDIT_REPL_IMPORTED'`
- `summary = remote_event_hash`

Si existe, se omite.

## Flujo mínimo

1. nodo origen crea tarea `audit_replicate_range`;
2. tarea se envía al peer y queda en `node_task`;
3. peer acepta, exporta bloque mínimo y responde `AUDIT_BLOCK ...`;
4. origen recibe, importa y persiste referencias deduplicadas;
5. ambos lados auditan estado de replicación.

## Eventos relevantes

- `AUDIT_REPLICATION_STARTED`
- `AUDIT_REPLICATION_RECEIVED`
- `AUDIT_REPLICATION_PERSISTED`
- `AUDIT_REPLICATION_FAILED`
