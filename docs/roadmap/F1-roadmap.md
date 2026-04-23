# F1 Roadmap — VitaOS con IA

## Meta del milestone

Arrancar VitaOS en x86_64 UEFI, operar en RAM, montar auditoría persistente en SQLite desde USB writable, presentar una consola textual guiada y permitir a la IA kernel-resident proponer acciones de sistema y de red.

## F1A

1. Boot UEFI x86_64.
2. Consola mínima.
3. Temporizador y reloj monotónico.
4. Descubrimiento de CPU, RAM y firmware.
5. Enumeración básica de block/USB.
6. Identificación del destino de auditoría.
7. Apertura del backend SQLite.
8. Registro de `boot_session` y `audit_event`.
9. Propuesta inicial de IA.
10. Pantalla guiada bilingüe.

## F1B

1. Ethernet básica.
2. Descubrimiento VitaNet v0.
3. Tabla `node_peer`.
4. Tabla `node_task`.
5. Heartbeat.
6. Replicación primaria de auditoría.

## F1C

1. Port inicial a ARM64.
2. Revalidación del flujo F1A.
3. Revalidación de VitaNet.
4. Revalidación del modelo de auditoría.

## Criterios de aceptación

- Arranque reproducible en QEMU x86_64 UEFI.
- Sin auditoría persistente, el sistema queda en `RESTRICTED_DIAGNOSTIC`.
- Con auditoría válida, el sistema entra en `OPERATIONAL`.
- La consola muestra hardware detectado y propuestas activas.
- VitaNet puede descubrir al menos un peer en red local de prueba.
