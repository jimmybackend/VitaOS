# VitaOS architecture tracks

Este documento deja claro desde dónde continúa VitaOS después de la integración inicial de VitaIR-Tri.

La meta es evitar mezclar dos necesidades distintas:

1. seguir construyendo el sistema operativo propio de VitaOS;
2. abrir una ruta práctica basada en Linux para tener red, navegador, accesibilidad e IA remota más pronto.

Ambas rutas son válidas, pero no deben confundirse.

---

## Resumen

VitaOS queda organizado en dos tracks complementarios:

1. **Native VitaOS Core**
2. **Linux-Assisted VitaOS**

El primer track protege el objetivo original: un sistema propio, UEFI/live-first, text-first, audit-first y útil en emergencia incluso sin un sistema operativo tradicional.

El segundo track permite una variante práctica basada en Linux para escenarios donde conviene usar drivers, red, navegador y herramientas ya disponibles, sin abandonar el core nativo.

---

## Track 1 — Native VitaOS Core

### Propósito

Native VitaOS Core es la línea principal del proyecto.

Su objetivo es construir VitaOS como sistema propio, orientado a:

- boot UEFI;
- operación live/RAM;
- consola textual;
- uso en emergencia;
- auditoría persistente;
- storage `/vita`;
- sesiones persistentes;
- exports TXT/JSONL;
- VitaIR-Tri;
- diagnóstico local;
- eventual ISO/USB real.

Este track no depende de Linux para arrancar.

### Enfoque

Native VitaOS Core debe seguir siendo:

- **UEFI/live-first**: pensado para arrancar directamente desde firmware.
- **Text-first**: consola textual clara, usable y auditable.
- **Audit-first**: toda capacidad importante debe dejar rastro.
- **Emergency-first**: prioriza utilidad real en escenarios críticos.
- **Offline-capable**: debe seguir siendo útil aunque no haya red.
- **Honesto**: no debe afirmar capacidades no implementadas.

### Componentes relevantes actuales

Este track incluye o se relaciona con:

- storage persistente `/vita`;
- árbol persistente de sesiones y reportes;
- journal TXT/JSONL;
- export de última sesión;
- diagnostic bundle;
- selftest;
- `vitair-state.jsonl`;
- VitaIR-Tri como representación interna de claims auditables;
- consola local;
- editor seguro;
- validadores;
- preparación futura de ISO/USB real.

### Reglas de honestidad técnica

Native VitaOS Core no debe afirmar que ya tiene:

- red real completa en hardware UEFI;
- Wi-Fi real completa;
- SQLite persistente completa en UEFI;
- IA local completa;
- AWS Bedrock real;
- Hosted AI Bridge real;
- GUI;
- navegador;
- userland amplio.

Estas capacidades solo deben documentarse como pendientes, futuras o experimentales hasta que existan y estén validadas.

### Reglas de implementación

En este track:

- no introducir dependencias Linux-specific dentro del core UEFI;
- no usar `stdio` en rutas UEFI/freestanding;
- no usar memoria dinámica salvo fase explícita;
- no tocar `storage_bootstrap_persistent_tree()` sin justificación fuerte;
- no tocar `schema/audit.sql` sin fase explícita de schema;
- no romper JSONL;
- no romper rotación de sesiones;
- no romper last-session export;
- no mezclar muchas features en un solo PR.

### Siguiente trabajo recomendado

Después de documentar esta bifurcación, un siguiente PR posible para este track sería:

```text
docs/architecture/native-vitaos-roadmap.md
```

---

## Track 2 — Linux-Assisted VitaOS

### Propósito

Linux-Assisted VitaOS es una ruta paralela y práctica para acelerar capacidades que requieren un ecosistema maduro de drivers y userland.

Está orientada a disponer antes de:

- red estable;
- navegador;
- accesibilidad moderna;
- integración con IA remota;
- herramientas de soporte y operación cotidiana.

### Alcance

Este track puede ejecutar componentes de VitaOS sobre una base Linux, siempre que:

- mantenga el lenguaje operativo y principios de VitaOS;
- conserve trazabilidad de decisiones y acciones;
- no reemplace ni bloquee la evolución del core nativo;
- haga explícitas sus dependencias del host Linux.

### Guardrails

Linux-Assisted VitaOS debe:

- etiquetar claramente qué funciones dependen de Linux;
- mantener compatibilidad de formatos de auditoría (TXT/JSONL cuando aplique);
- evitar claims de "equivalencia total" con el boot nativo UEFI;
- respetar la disciplina audit-first para acciones relevantes;
- documentar límites offline cuando la función dependa de red o servicios remotos.

### Relación con Native VitaOS Core

Esta ruta **no** cambia la prioridad estratégica del proyecto:

- Native VitaOS Core sigue siendo la línea principal.
- Linux-Assisted VitaOS reduce tiempo de llegada a capacidades prácticas.
- Los aprendizajes de UX, auditoría y operación deben retroalimentar el core nativo cuando sea viable.

### Siguiente trabajo recomendado

Después de documentar esta bifurcación, un siguiente PR posible para este track sería:

```text
docs/architecture/linux-assisted-vitaos-roadmap.md
```
