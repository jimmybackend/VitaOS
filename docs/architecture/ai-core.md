# AI Core — VitaOS con IA

## Definición

La IA de VitaOS F1 no es un LLM genérico dentro del kernel. Es un núcleo de decisión residente compuesto por:

- percepción;
- evaluación;
- propuesta;
- coordinación.

## Entradas

- estado del sistema;
- hardware detectado;
- estado de auditoría;
- peers;
- tipo de sesión;
- comandos del usuario;
- knowledge packs cargados.

## Salidas

- propuestas visibles;
- preguntas críticas;
- acciones sugeridas;
- registros en auditoría;
- solicitudes a VitaNet;
- cambios de prioridad.

## Formato operativo esperado

1. resumen entendido
2. riesgos inmediatos
3. preguntas críticas
4. acciones inmediatas sugeridas
5. recursos disponibles
6. propuesta del sistema
7. estado de auditoría

## Restricciones

- No debe inventar capacidades del hardware.
- No debe ejecutar acciones críticas sin canal explícito definido.
- Toda propuesta debe dejar rastro en `ai_proposal`.
- Toda respuesta humana asociada debe dejar rastro en `human_response`.

## Tareas para Codex

- diseñar estructuras `ai_context_t`, `ai_observation_t`, `ai_proposal_t`;
- enlazar `ai_core.c` con `proposal.c`, `audit_core.c`, `node_core.c` y `emergency_core.c`;
- mantener salida bilingüe simple.
