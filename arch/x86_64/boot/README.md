# x86_64 boot

Implementación actual (slice F1A):
- entrada UEFI real (`efi_main`) en `uefi_entry.c`;
- enlace de consola temprana UEFI al kernel;
- creación de `vita_handoff_t` explícito;
- llamada directa a `kmain(&handoff)`.

Contrato de handoff:
- definido en `include/vita/boot.h`.

Cómo probar:
1. `make`
2. `make run`
3. `make smoke`
