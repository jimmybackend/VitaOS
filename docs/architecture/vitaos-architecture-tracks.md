# VitaOS architecture tracks

VitaOS tiene dos lineas de arquitectura complementarias. Este documento define donde comenzar despues de la integracion inicial de VitaIR-Tri y evita mezclar objetivos de sistema operativo propio con objetivos practicos basados en Linux.

## Proposito

El proyecto mantiene como objetivo principal construir un sistema util en emergencia, honesto sobre sus capacidades reales, auditable y text-first. Para avanzar sin confundir el alcance, VitaOS se organiza en dos tracks:

1. **Native VitaOS Core**: el sistema propio, live/UEFI-first, audit-first y orientado a uso aun sin un OS tradicional.
2. **Linux-Assisted VitaOS**: una variante practica basada en Linux para acelerar navegador, red, drivers, accesibilidad e IA remota cuando exista conectividad.

Linux-Assisted VitaOS no reemplaza al core nativo. Es un companero operativo para escenarios donde Linux ya puede aportar hardware, red y herramientas de usuario mientras el core propio madura.

## Estado real actual

El estado documentado aqui debe seguir la regla de honestidad tecnica del proyecto: no afirmar capacidades que aun no esten implementadas.

Implementado o documentado en el slice actual:

- Boot UEFI y hosted.
- Persistencia real sobre el arbol `/vita` cuando existe backend writable.
- Reportes TXT/JSONL.
- Historial persistente por sesion.
- Editor seguro.
- Validadores del flujo actual.
- VitaIR-Tri como representacion interna y auditable de claims ternarios.
- Comandos y reportes que exponen VitaIR-Tri, incluyendo `status`, `audit`, `storage status`, `selftest`, `diagnostic`, `export vitair` y `export vitair-state`.

No implementado todavia:

- SQLite persistente completa en UEFI.
- Red/Wi-Fi real completa en hardware UEFI.
- Integracion real con AWS Bedrock.
- IA local completa.
- Hosted AI Bridge de produccion.
- GUI o navegador dentro del core nativo.
- ISO Linux de VitaOS.

## Track 1: Native VitaOS Core

### Objetivo

Seguir construyendo VitaOS como sistema propio, live-first, UEFI-first, text-first y audit-first. Este track protege el nucleo independiente del proyecto.

### Prioridades

- Mantener el core propio y freestanding-friendly.
- No depender de Linux para arrancar.
- Preservar consola local y flujo textual guiado.
- Fortalecer storage persistente, auditoria, sesiones, exports y diagnisticos.
- Mantener VitaIR-Tri como lenguaje interno para expresar estado operativo auditable.
- Preparar ISO/USB real cuando se entre en fase explicita de validacion de hardware.
- Mantener modo emergencia util aun sin red.

### Reglas de este track

- No introducir dependencias Linux-specific dentro del core UEFI si contaminan el diseno.
- No usar `stdio` en rutas UEFI/freestanding.
- No usar memoria dinamica salvo fase explicita.
- No tocar `storage_bootstrap_persistent_tree()` sin justificacion fuerte.
- No tocar `schema/audit.sql` sin una fase explicita de schema.
- No romper JSONL, rotacion de sesiones ni last-session export.
- No prometer red, AWS, IA local completa ni SQLite UEFI completa hasta que existan.

### Siguiente comienzo recomendado

El siguiente paso recomendado para este track es un PR pequeno de documentacion o checklist nativo, por ejemplo:

```text
docs/architecture/native-vitaos-roadmap.md
```

Ese documento debe listar:

- lo ya implementado;
- lo pendiente;
- los riesgos tecnicos;
- las reglas que no se deben romper;
- el criterio para entrar a fase ISO/USB real.

## Track 2: Linux-Assisted VitaOS

### Objetivo

Crear una variante practica basada en Linux que pueda arrancar como live ISO o rescue OS y ayudar antes de que el sistema propio tenga todo el soporte de hardware, red, navegador e IA remota.

### Diferencia contra Native VitaOS Core

Native VitaOS Core busca independencia tecnica y control del boot. Linux-Assisted VitaOS busca utilidad practica temprana usando una base Linux existente.

Linux-Assisted VitaOS puede tener navegador, red, drivers, herramientas de diagnostico, accesibilidad y cliente de IA remota cuando haya internet. Aun asi, debe consumir estado real de VitaOS y no inventar disponibilidad.

### Requisitos minimos esperados

- Arranque live basado en Linux.
- Red cableada y Wi-Fi mediante soporte de la distro base.
- Navegador o interfaz web disponible.
- Herramientas de montaje/copia para acceder a `/vita`.
- Lectura de reportes TXT/JSONL.
- Lectura de `/vita/export/reports/vitair-state.jsonl` cuando exista.
- Generacion de diagnosticos del entorno Linux-assisted.
- Modo emergencia guiado.

### Relacion con `/vita` y VitaIR-Tri

Linux-Assisted VitaOS debe tratar `/vita` como fuente de verdad persistente cuando este disponible. En particular:

- debe leer `vitair-state.jsonl` si existe;
- debe interpretar `state` solo como `1`, `0` o `-1` en JSONL;
- puede mostrar `+1`, `0` o `-1` en salida humana;
- debe mantener `severity` como informacion independiente de `state`;
- no debe inventar que SQLite, red, IA remota o storage estan disponibles si los claims no lo respaldan.

### IA remota

La IA remota en este track debe operar como asistente conectada cuando haya red. Debe consumir diagnosticos, exports y claims VitaIR-Tri. No debe sustituir la auditoria ni afirmar estado no medido.

### Riesgos

- Confundir la variante Linux con el sistema operativo propio.
- Introducir dependencias Linux-specific en el core nativo.
- Prometer una ISO Linux de VitaOS antes de construirla.
- Dejar que la IA remota invente estado operativo.
- Duplicar formatos de auditoria en vez de consumir los existentes.

### Siguiente comienzo recomendado

El siguiente paso recomendado para este track es documentar arquitectura antes de escribir codigo:

```text
docs/architecture/linux-assisted-vitaos.md
```

Ese documento debe explicar proposito, requisitos minimos, consumo de `/vita`, consumo de VitaIR-Tri, limites, riesgos, roadmap y que no esta implementado todavia.

## Punto de inicio recomendado ahora

Despues de PR #52, el mejor comienzo es documentar esta bifurcacion arquitectonica antes de tocar codigo. Este documento funciona como PR base para separar las metas:

- **Native VitaOS Core** sigue siendo la linea principal del sistema propio.
- **Linux-Assisted VitaOS** queda como linea companera para utilidad practica con Linux, red, navegador e IA remota.

Con esto, los proximos PRs pueden avanzar sin mezclar responsabilidades.

## Politica de pruebas para cambios docs-only

Para PRs que solo agregan o modifican documentacion:

```bash
git diff
rg "Linux-Assisted|Native VitaOS|VitaIR-Tri" docs/architecture README.md
git status
```

No se requiere `make hosted`, `make`, validadores largos ni `make iso` si el cambio es estrictamente documental.

## Politica de pruebas para PRs pequenos de C

Para cambios pequenos de C:

```bash
make hosted
make
```

Agregar una prueba manual focalizada al comando o flujo afectado.

No ejecutar validadores largos ni `make iso` salvo fase explicita.

## Politica para fase ISO/USB real

Cuando se decida entrar en preparacion ISO/USB real, usar una fase explicita con validacion completa:

```bash
make clean
make hosted
make smoke-audit
make
./tools/test/validate-console-editor-history.sh
./tools/test/validate-storage-persistence.sh
./tools/test/validate-vitaos.sh
make iso
```

Despues de eso, realizar prueba USB real y auditar el contenido persistente de `/vita` antes de avanzar a red, Hosted AI Bridge o integraciones remotas.
