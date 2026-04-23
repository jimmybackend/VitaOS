# Arch — VitaOS con IA

La capa de arquitectura debe ser delgada.

## Reglas

- No mover lógica de negocio aquí.
- Mantener boot, MMU, IRQ, timer y consola temprana lo más aislados posible.
- Compartir el máximo posible en `kernel/`.

## Objetivo F1

- x86_64 UEFI primero
- arm64 preparado como siguiente port
