# Boot Flow — VitaOS con IA

## Objetivo

Documentar el flujo de arranque base para F1A en x86_64 UEFI.

## Flujo

1. Firmware UEFI entrega control al boot stage.
2. El boot stage prepara memoria mínima, stack y handoff structure.
3. Se invoca `kmain()`.
4. `kmain()` inicia consola temprana.
5. Se inicializa reloj monotónico.
6. Se ejecuta `memory_early_init()`.
7. Se crea el contexto de boot (`boot_context_t`).
8. Se entra a `HW_DISCOVERY`.
9. Se busca el destino de auditoría.
10. Si SQLite abre correctamente:
   - se registra `BOOT_OK`;
   - el sistema pasa a `OPERATIONAL`.
11. Si no abre:
   - se registra en buffer temporal;
   - el sistema pasa a `RESTRICTED_DIAGNOSTIC`.

## Contratos

- Ningún subsistema debe asumir almacenamiento persistente antes de `audit_init()`.
- La consola temprana debe estar disponible antes de fallos críticos.
- El boot stage no debe codificar políticas de negocio; solo preparar el handoff.

## Tareas de implementación

- Definir `boot_context_t`.
- Definir `arch/x86_64/boot/` con loader mínimo.
- Definir salida por framebuffer o consola serial temprana.
