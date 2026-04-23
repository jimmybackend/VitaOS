# Tests — VitaOS con IA

## Smoke test de boot UEFI

`tools/test/smoke-boot.sh` valida banner de arranque cuando QEMU/OVMF están disponibles.

## Smoke test de auditoría + VitaNet F1B (tasks + replicación)

`tools/test/smoke-audit.sh` valida:
- DB de nodo A y DB de peer B (harness);
- descubrimiento/persistencia de peers en `node_peer`;
- propuesta de enlace + approve;
- uso real de `node_task` con `peer_status_request` y `audit_replicate_range`;
- transición de estados (`created/sent/accepted/done`);
- replicación mínima bidireccional de auditoría (`AUDIT_BLOCK` ida/vuelta);
- importación deduplicada (`AUDIT_REPL_IMPORTED`);
- trazabilidad de eventos de tarea/replicación.

Ejecución:

```bash
make smoke-audit
```
