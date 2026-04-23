# Boot Flow — VitaOS con IA (vertical slice F1A)

## Objetivo

Definir un camino ejecutable y reproducible desde boot hasta `kmain()`, con auditoría persistente mínima funcional validable.

## Flujo implementado

1. UEFI carga `EFI/BOOT/BOOTX64.EFI` (ruta UEFI actual).
2. El boot stage construye `vita_handoff_t` con contrato explícito.
3. El boot stage invoca `kmain(&handoff)`.
4. `kmain()` valida handoff y emite eventos tempranos en buffer.
5. `kmain()` inicializa backend persistente de auditoría.
6. Si auditoría falla: panic y modo no-operativo.
7. Si auditoría está lista:
   - aplica esquema SQL;
   - crea `boot_session`;
   - flushea eventos tempranos;
   - emite `AUDIT_BACKEND_READY`;
   - muestra banner.

## Contrato de handoff

Archivo: `include/vita/boot.h`

```c
typedef struct {
    uint64_t magic;
    uint32_t version;
    uint32_t size;

    const char *arch_name;
    uint64_t firmware_type;
    const char *audit_db_path;

    void *uefi_image_handle;
    void *uefi_system_table;
} vita_handoff_t;
```

## Build y validación reproducible

- `make` (artefacto UEFI)
- `make hosted` (runner hosted para validar SQLite)
- `make smoke` (smoke de banner en QEMU cuando disponible)
- `make smoke-audit` (creación/validación de DB y hash chain)

## Nota de alcance

- SQLite persistente funcional está validado en flujo hosted F1A.
- La ruta UEFI mantiene la compacidad freestanding mientras se completa la integración VFS de almacenamiento writable para firmware.
