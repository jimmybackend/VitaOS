# F1A Gap Analysis — Estado del repositorio

Fecha de revisión: 2026-04-23

## Resumen ejecutivo

El repositorio está en etapa **fundacional**: tiene especificación, contratos y esqueletos de módulos, pero aún no implementa el flujo mínimo ejecutable de F1A.

Conclusión: para “terminarlo” (alcanzar F1A) faltan principalmente implementaciones de boot x86_64 UEFI, backend SQLite persistente, consola guiada real, descubrimiento de hardware y red mínima, y pruebas automatizadas de aceptación.

## Qué sí está definido

- Visión y límites de fase F1/F1A.
- Modelo de estados del sistema.
- Contrato de auditoría SQLite.
- Estructura del kernel modular y stubs por subsistema.

## Brechas críticas para cerrar F1A

1. **Boot x86_64 UEFI real**
   - Definir ABI de handoff a `kmain`.
   - Construir imagen EFI booteable y flujo reproducible en QEMU.

2. **Auditoría persistente funcional**
   - `audit_init_persistent_backend()` hoy no inicializa persistencia.
   - Implementar VFS SQLite kernel y aplicación de esquema.
   - Registrar `boot_session` + primeros `audit_event` con hash encadenado.

3. **Descubrimiento de hardware (real, no placeholder)**
   - CPU/RAM/firmware/USB/storage/network con datos reales.
   - Volcar `hardware_snapshot` en auditoría.

4. **Consola textual guiada bilingüe operativa**
   - Renderizar estado real.
   - Menú inicial de operación y entrada de usuario.

5. **AI core de propuestas (MVP)**
   - Pasar de “evento de encendido” a pipeline mínimo:
     percepción → evaluación → propuesta estructurada → auditoría.

6. **Ciclo de propuestas y respuesta humana**
   - Crear/presentar/aceptar/rechazar propuestas.
   - Persistir en `ai_proposal` y `human_response`.

7. **Nodo cooperativo inicial (descubrimiento mínimo)**
   - Inicializar red básica y detectar al menos un peer en entorno de prueba.
   - Registrar `node_peer` y estados de enlace.

8. **Pruebas y herramientas de build/run**
   - Scripts de build faltantes y smoke tests de F1A.
   - Criterios de aceptación de roadmap aún no automatizados.

## Lenguajes recomendados para este kernel con IA

- **C (principal):** núcleo, subsistemas comunes, drivers mínimos.
- **Assembly (mínimo necesario):** arranque/entrada de arquitectura e interrupciones críticas.
- **SQL:** contrato y validaciones de auditoría (SQLite).
- **Shell/Python (herramientas):** build, empaquetado live, ejecución de pruebas/QEMU.

Nota: la “IA” en F1 está planteada como **motor residente de decisión** (no un LLM pesado dentro del kernel), por lo que sigue siendo implementación en C con plantillas/reglas y estado auditado.

## Orden sugerido de ejecución (ruta corta a F1A)

1. Boot x86_64 UEFI + handoff estable.
2. Consola temprana + reloj monotónico.
3. SQLite VFS + `boot_session` + `audit_event` encadenado.
4. Hardware discovery + `hardware_snapshot`.
5. AI proposal MVP + persistencia de propuestas/respuestas.
6. Consola guiada bilingüe conectada al estado real.
7. Red mínima + descubrimiento de peer en test.
8. QEMU smoke tests y gating de aceptación F1A.
