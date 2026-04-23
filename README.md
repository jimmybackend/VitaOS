# VitaOS con IA

VitaOS con IA es un sistema operativo desde cero, orientado a modo live/RAM, con una IA residente en el kernel, auditoría obligatoria en SQLite y nodos cooperativos para ayuda, coordinación y operación en escenarios de emergencia.

## Estado actual

Este repositorio es la base de arranque del proyecto. Incluye:

- documentación fundacional del sistema;
- reglas de trabajo para agentes de IA y Codex;
- esquema inicial de auditoría en SQLite;
- árbol de módulos del kernel y de arquitectura;
- stubs y contratos de implementación para cada subsistema;
- knowledge packs iniciales bilingües;
- documentación de boot, IA, auditoría y VitaNet.

## Objetivo del milestone F1A

- Boot x86_64 UEFI.
- Consola textual guiada.
- Detección de hardware básica.
- Auditoría persistente SQLite en USB writable.
- Núcleo de IA residente con propuestas visibles.
- Descubrimiento inicial de nodos VitaOS.

## Regla central del proyecto

Sin auditoría persistente válida, VitaOS no entra en modo operativo completo.

## Estructura principal

```text
vitaos/
├── README.md
├── AGENTS.md
├── docs/
├── kernel/
├── arch/
├── drivers/
├── proto/
├── knowledge/
├── schema/
├── tools/
└── tests/
```

## Flujo recomendado de trabajo

1. Leer `docs/VitaOS-SPEC.md`.
2. Leer `AGENTS.md`.
3. Revisar `schema/audit.sql`.
4. Implementar primero F1A en `arch/x86_64/` y `kernel/`.
5. Mantener toda nueva capacidad sincronizada con auditoría.
6. No agregar features fuera del milestone.

## Documentos clave

- `docs/VitaOS-SPEC.md`
- `docs/architecture/boot-flow.md`
- `docs/architecture/ai-core.md`
- `docs/audit/audit-model.md`
- `docs/audit/sqlite-contract.md`
- `docs/roadmap/F1-roadmap.md`
- `proto/vitanet/README.md`

## Licencia sugerida

MIT o Apache-2.0. La decisión final queda pendiente.

## Nota para Codex

Este repositorio contiene stubs intencionales. Antes de generar código, Codex debe:
- identificar el módulo objetivo;
- preservar el contrato de auditoría;
- explicar supuestos;
- producir código completo cuando se le pida;
- respetar el milestone actual.
