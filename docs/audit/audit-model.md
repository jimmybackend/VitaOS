# Audit Model — VitaOS con IA

## Principio

Sin auditoría persistente válida, VitaOS no entra en modo operativo completo.

## Fuente de verdad

`schema/audit.sql` es el contrato canónico del modelo de auditoría.

## Implementación mínima funcional (F1A actual)

Para el slice actual, el backend persistente usa SQLite en flujo hosted de validación.

- DB por defecto: `build/audit/vitaos-audit.db`
- Esquema aplicado automáticamente desde `schema/audit.sql` al iniciar.
- Cada boot persistente crea:
  - 1 fila en `boot_session`
  - eventos iniciales en `audit_event` con hash chain

Eventos iniciales esperados:
1. `BOOT_STARTED`
2. `HANDOFF_TO_KMAIN`
3. `CONSOLE_READY`
4. `AUDIT_BACKEND_READY`
5. `HW_DISCOVERY_STARTED`
6. `HW_DISCOVERY_COMPLETED`
7. `HW_SNAPSHOT_PERSISTED`
8. `GUIDED_CONSOLE_SHOWN`
9. `MENU_OPTION_SELECTED`
10. `EMERGENCY_SESSION_STARTED`
11. `EMERGENCY_INPUT_RECEIVED`
12. `STRUCTURED_RESPONSE_SHOWN`

## Reglas

1. Todo cambio importante debe emitir al menos un `audit_event`.
2. Toda propuesta de IA debe reflejarse en `ai_proposal`.
3. Toda respuesta humana debe reflejarse en `human_response`.
4. Todo peer descubierto debe persistirse en `node_peer`.
5. Toda tarea entre nodos debe persistirse en `node_task`.
6. Toda carga de knowledge pack debe persistirse en `knowledge_pack`.

## Integridad

Cada evento calcula `event_hash` con encadenamiento sobre `prev_hash`.

Payload actual del hash:

`boot_id || event_seq || event_type || severity || actor_type || summary || details_json || monotonic_ns || prev_hash`

## Validación mínima reproducible

```bash
make smoke-audit
```

El smoke test valida:
- existencia de `boot_session`;
- existencia de `audit_event` iniciales;
- `hardware_snapshot` persistido;
- continuidad de `event_seq`;
- continuidad de `prev_hash`;
- recomputación de `event_hash`.

## Inspección manual rápida

```bash
sqlite3 build/audit/vitaos-audit.db "select boot_id, arch, boot_unix from boot_session;"
sqlite3 build/audit/vitaos-audit.db "select event_seq, event_type, prev_hash, event_hash from audit_event order by id;"
sqlite3 build/audit/vitaos-audit.db "select cpu_arch,cpu_model,ram_bytes,firmware_type,console_type,net_count,storage_count,usb_count,wifi_count from hardware_snapshot order by id desc limit 1;"
```
