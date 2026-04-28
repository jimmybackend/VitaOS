# VitaOS con IA

VitaOS con IA es un sistema operativo desde cero, orientado a operación live/RAM, con una IA residente en el kernel, auditoría obligatoria en SQLite y nodos cooperativos para ayuda, coordinación y operación en escenarios de emergencia.

## Estado del repositorio

El repositorio ya contiene un slice ejecutable mínimo para el milestone actual, centrado en cerrar una base real de F1A/F1B sin agregar features fuera de alcance.

### Slice hoy integrado

- build **UEFI x86_64**;
- build **hosted** para validación rápida;
- `kmain()` integrado con:
  - validación de handoff;
  - arranque temprano de consola;
  - detección básica de hardware;
  - auditoría persistente en SQLite en modo hosted;
  - motor inicial de propuestas;
  - consola guiada mínima;
  - VitaNet hosted mínimo;
  - peer discovery básico;
  - replicación básica de bloque reciente de auditoría;
- smoke tests para boot y auditoría.

## Alcance real del milestone actual

### F1A base integrada
- arranque x86_64 UEFI;
- consola textual mínima;
- detección básica de hardware;
- estructura de auditoría persistente;
- proposals visibles;
- flujo guiado inicial.

### F1B mínimo real
- VitaNet hosted mínimo operativo;
- descubrimiento básico de peer;
- persistencia de `node_peer`;
- propuesta de vinculación inicial;
- exportación básica de bloque reciente de auditoría;
- base mínima para `node_task`.

## Regla central

**Sin auditoría persistente válida, VitaOS no debe entrar en modo operativo completo.**

Ese comportamiento se mantiene como regla del proyecto.

## Estado operativo real por modo

### EFI / UEFI
Hoy el build EFI sirve para validar:
- arranque;
- consola temprana;
- banner;
- paso por `kmain()`.

Actualmente el backend persistente completo de auditoría no está operativo en la ruta UEFI/freestanding, así que ese camino debe entenderse como **boot + diagnóstico restringido**, no como operación completa.

### Hosted
El modo hosted es hoy la ruta principal para validar:
- SQLite persistente;
- `boot_session`;
- `hardware_snapshot`;
- `audit_event` con hash chain;
- `ai_proposal`;
- `human_response`;
- `node_peer`;
- VitaNet hosted mínimo;
- smoke test ampliado.

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

## Archivos clave del slice actual

- `Makefile`
- `arch/x86_64/boot/uefi_entry.c`
- `arch/x86_64/boot/host_entry.c`
- `kernel/kmain.c`
- `kernel/console.c`
- `kernel/hardware_discovery.c`
- `kernel/proposal.c`
- `kernel/node_core.c`
- `kernel/audit_core.c`
- `include/vita/console.h`
- `include/vita/audit.h`
- `include/vita/node.h`
- `include/vita/proposal.h`
- `schema/audit.sql`
- `tools/test/smoke-boot.sh`
- `tools/test/smoke-audit.sh`

## Requisitos

### Para EFI / QEMU
- `clang`
- `lld`
- `qemu-system-x86_64`
- OVMF, por ejemplo:
  - `/usr/share/OVMF/OVMF_CODE.fd`
  - `/usr/share/OVMF/OVMF_VARS.fd`

### Para hosted
- `clang`
- `sqlite3`
- `libsqlite3`
- `sha256sum` (coreutils)

## Build

### Build EFI

```bash
make
```

Salida esperada:

```text
build/efi/EFI/BOOT/BOOTX64.EFI
```

### Build hosted

```bash
make hosted
```

Salida esperada:

```text
build/hosted/vitaos-hosted
```

## Ejecución

### EFI en QEMU

```bash
make run
```

### Hosted

```bash
./build/hosted/vitaos-hosted
```

O indicando explícitamente la base SQLite:

```bash
./build/hosted/vitaos-hosted build/audit/vitaos-audit.db
```

## Smoke tests

### Smoke de boot EFI

```bash
make smoke
```

Valida al menos:
- `VitaOS with AI / VitaOS con IA`
- `Boot: OK`
- `Arch: x86_64`
- `Console: OK`

### Smoke de auditoría + proposals + VitaNet hosted

```bash
make smoke-audit
```

Valida:
- creación de `boot_session`;
- creación de `hardware_snapshot`;
- cadena hash de `audit_event`;
- proposals iniciales;
- peer básico descubierto;
- intento de replicación de auditoría;
- presencia de tablas relevantes del slice actual.

## Verificación manual de la base SQLite

### Sesión de boot

```bash
sqlite3 build/audit/vitaos-audit.db "select boot_id,node_id,arch,boot_unix from boot_session;"
```

### Eventos de auditoría

```bash
sqlite3 build/audit/vitaos-audit.db "select event_seq,event_type,summary,prev_hash,event_hash from audit_event order by id;"
```

### Snapshot de hardware

```bash
sqlite3 build/audit/vitaos-audit.db "select cpu_arch,cpu_model,ram_bytes,firmware_type,console_type,net_count,storage_count,usb_count,wifi_count from hardware_snapshot order by id desc limit 1;"
```

### Proposals

```bash
sqlite3 build/audit/vitaos-audit.db "select proposal_id,proposal_type,status from ai_proposal order by rowid;"
```

### Peers

```bash
sqlite3 build/audit/vitaos-audit.db "select peer_id,transport,trust_state,link_state from node_peer;"
```

## Flujo funcional esperado hoy

En hosted, el flujo esperado del slice actual es:

1. boot y validación de handoff;
2. activación de backend SQLite;
3. creación de `boot_session`;
4. detección de hardware;
5. persistencia de `hardware_snapshot`;
6. generación de proposals iniciales;
7. presentación de consola guiada mínima;
8. inicio de VitaNet hosted;
9. descubrimiento de peer y persistencia de `node_peer`;
10. intento de replicación básica de bloque reciente de auditoría.

## Comandos de consola ya contemplados en el slice

La consola guiada mínima ya contempla o prepara el flujo para comandos como:

- `status`
- `hw`
- `audit`
- `peers`
- `proposals`
- `emergency`
- `helpme`
- `approve <id>`
- `reject <id>`
- `shutdown`

## Límites actuales conocidos

Para mantener disciplina de milestone, todavía **no** se debe asumir que ya existen:

- GUI completa;
- backend persistente UEFI completo;
- protocolo VitaNet completo;
- replicación bidireccional completa;
- scheduler avanzado;
- userland amplio;
- control completo de `node_task`.

## Disciplina de desarrollo

Este repositorio se está reparando y consolidando archivo por archivo.

Reglas de trabajo activas:
- no inventar features fuera del milestone;
- respetar el esquema real de auditoría;
- mantener compatibilidad con el código existente;
- priorizar archivos completos listos para copiar y pegar;
- mantener el foco en F1A/F1B mínimo real.

## Documentos recomendados

- `docs/VitaOS-SPEC.md`
- `docs/history/applied-patches.md`
- `docs/tools/no-python-runtime.md`
- `docs/validation/f1a-f1b-validation-checklist.md`
- `AGENTS.md`
- `schema/audit.sql`

## Nota para asistentes de IA y Codex

Antes de generar o modificar código:
1. identificar el módulo exacto;
2. respetar el milestone actual;
3. no asumir APIs o estructuras inexistentes;
4. preservar la trazabilidad de auditoría;
5. devolver código completo cuando se pida.

## Política no-Python

VitaOS no usa Python como dependencia de runtime/build obligatorio del sistema.


Ver: `docs/tools/no-python-runtime.md`.
