# Emergency Flow — Guided Console F1A

## Objetivo

Definir el flujo mínimo real de emergencia en consola textual bilingüe, conectado a estado del sistema, auditoría persistente y motor de propuestas.

## Entrada

Desde el modo hosted, la consola guiada acepta:

- `helpme` (solicita texto libre)
- `emergency <texto libre>`

## Pipeline mínimo

1. **Inicio de sesión de emergencia**
   - evento: `EMERGENCY_SESSION_STARTED`
2. **Recepción de texto libre**
   - evento: `EMERGENCY_INPUT_RECEIVED`
3. **Clasificación simple por reglas** (sin LLM pesado)
   - patrones es/en básicos: `help`, `emerg`, `sang`, `dolor`, `fuego`, `rescue`, `auxilio`, `accident`
4. **Respuesta estructurada operativa**
   - 1) resumen entendido
   - 2) riesgos inmediatos
   - 3) preguntas críticas
   - 4) acciones inmediatas sugeridas
   - 5) recursos del sistema disponibles
   - 6) propuesta de acción del sistema
   - 7) estado de auditoría
   - evento: `STRUCTURED_RESPONSE_SHOWN`

## Estado real consumido

La respuesta usa estado real del arranque actual:

- estado de auditoría persistente (`audit_get_runtime`)
- snapshot de hardware (`vita_hw_snapshot_t`)
- propuestas pendientes (`proposal_count_pending`)

## Auditoría relacionada de consola guiada

Además del flujo de emergencia, la consola emite:

- `GUIDED_CONSOLE_SHOWN`
- `MENU_OPTION_SELECTED`

## Limitaciones F1A (explícitas)

- En UEFI freestanding no existe loop interactivo completo todavía.
- El modo guiado completo y validable se ejecuta en `VITA_HOSTED`.
- La clasificación es deliberadamente mínima para F1A.
