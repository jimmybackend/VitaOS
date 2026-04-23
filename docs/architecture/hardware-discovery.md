# Hardware Discovery — F1A mínimo real

## Objetivo

Detectar hardware mínimo útil y persistir `hardware_snapshot` asociado al `boot_id` de auditoría.

## Datos detectados (implementación actual)

En flujo hosted F1A (`VITA_HOSTED`):

- `cpu_arch`: vía `uname(2)`.
- `cpu_model`: primera coincidencia `model name` en `/proc/cpuinfo`.
- `ram_bytes`: `MemTotal` de `/proc/meminfo`.
- `firmware_type`: `hosted` o `uefi` según `handoff`.
- `console_type`: `stdio` o `uefi_text` según `handoff`.
- `net_count`: interfaces en `/sys/class/net` excluyendo `lo`.
- `storage_count`: dispositivos en `/sys/block` excluyendo `loop*` y `ram*`.
- `usb_count`: entradas relevantes en `/sys/bus/usb/devices`.
- `wifi_count`: interfaces con `wireless/` en `/sys/class/net/<if>/wireless`.

## Persistencia

`audit_persist_hardware_snapshot()` inserta en `hardware_snapshot` con:
- `boot_id` de la sesión actual,
- todos los campos del snapshot,
- `detected_at_unix`.

## Eventos de auditoría emitidos

1. `HW_DISCOVERY_STARTED`
2. `HW_DISCOVERY_COMPLETED`
3. `HW_SNAPSHOT_PERSISTED`

## Verificación rápida

```bash
make smoke-audit
sqlite3 build/audit/vitaos-audit.db "select cpu_arch,cpu_model,ram_bytes,firmware_type,console_type,net_count,storage_count,usb_count,wifi_count from hardware_snapshot order by id desc limit 1;"
```

## Nota de plataforma

En ruta UEFI freestanding, algunos datos siguen limitados hasta cerrar capa VFS/drivers de F1A.
No se marca como “completado” fuera de lo realmente detectable en esa ruta.
