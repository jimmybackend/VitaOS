# Tests — VitaOS con IA

## Smoke test de boot UEFI

`tools/test/smoke-boot.sh` valida banner de arranque cuando QEMU/OVMF están disponibles.

## Smoke test de auditoría persistente + consola guiada + VitaNet v0

`tools/test/smoke-audit.sh` valida:
- creación de DB;
- creación de `boot_session`;
- inserción de `audit_event` iniciales;
- continuidad de `event_seq`;
- continuidad de `prev_hash`;
- recomputación de `event_hash`;
- inserción de `hardware_snapshot` con `ram_bytes` > 0;
- inserción de `ai_proposal` y `human_response`;
- descubrimiento de peer y persistencia en `node_peer`;
- propuesta de enlace (`N-1`) y transición de `link_state` tras `approve`;
- evento de replicación mínima (`AUDIT_REPLICATION_ATTEMPTED`).

Ejecución:

```bash
make smoke-audit
```
