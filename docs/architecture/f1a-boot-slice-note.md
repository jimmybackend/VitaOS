# Nota técnica breve — decisiones del vertical slice boot F1A

## Decisiones

1. **UEFI app única en esta etapa**
   - Para asegurar un camino real y corto, el kernel mínimo se enlaza dentro de `BOOTX64.EFI`.
   - Esto evita dependencias tempranas de un segundo stage loader en este milestone.

2. **Handoff explícito por estructura versionada**
   - Se define `vita_handoff_t` con `magic/version/size` para compatibilidad futura.
   - Se evita pasar datos implícitos por globales dispersas.

3. **Consola temprana con Simple Text Output**
   - El boot stage exporta un writer al kernel via `console_bind_writer()`.
   - El kernel conserva independencia del detalle UEFI y escribe texto plano.

4. **Reproducibilidad práctica**
   - `make` produce `BOOTX64.EFI`.
   - `make run` ejecuta en QEMU con OVMF.
   - `make smoke` valida texto esperado en salida serial.

## Límites conscientes

- Sin SQLite funcional todavía.
- Sin red ni discovery real de hardware.
- Sin GUI.

Esto mantiene el alcance en el objetivo pedido: boot real y verificable UEFI → kmain.
