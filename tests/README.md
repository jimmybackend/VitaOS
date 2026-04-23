# Tests — VitaOS con IA

## Smoke test de boot UEFI

`tools/test/smoke-boot.sh` valida banner de arranque cuando QEMU/OVMF están disponibles.

## Smoke test de auditoría persistente (SQLite)

`tools/test/smoke-audit.sh` valida:
- creación de DB;
- creación de `boot_session`;
- inserción de `audit_event` iniciales;
- continuidad de `event_seq`;
- continuidad de `prev_hash`;
- recomputación de `event_hash`.

Ejecución:

```bash
make smoke-audit
```
