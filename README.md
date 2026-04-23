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


## Vertical slice F1A (UEFI -> kmain)

Este repositorio ya incluye un camino ejecutable mínimo de arranque x86_64 UEFI hasta `kmain()` con consola temprana.

### Requisitos

- `clang` + `lld`
- `qemu-system-x86_64`
- OVMF en:
  - `/usr/share/OVMF/OVMF_CODE.fd`
  - `/usr/share/OVMF/OVMF_VARS.fd`

### Build

```bash
make
```

Salida esperada:
- `build/efi/EFI/BOOT/BOOTX64.EFI`

### Run (QEMU)

```bash
make run
```

### Hosted runner (validación SQLite)

```bash
make hosted
./build/hosted/vitaos-hosted build/audit/vitaos-audit.db
```

### Smoke test

```bash
make smoke
```

El smoke valida que aparezcan estas líneas:
- `VitaOS with AI / VitaOS con IA`
- `Boot: OK`
- `Arch: x86_64`
- `Console: OK`


### Smoke test de auditoría persistente (SQLite)

```bash
make smoke-audit
```

Base de datos generada por defecto:
- `build/audit/vitaos-audit.db`

Verificación manual:

```bash
sqlite3 build/audit/vitaos-audit.db "select boot_id, arch, boot_unix from boot_session;"
sqlite3 build/audit/vitaos-audit.db "select event_seq, event_type, prev_hash, event_hash from audit_event order by id;"
# eventos esperados incluyen BOOT/HANDOFF/CONSOLE/AUDIT/HW_DISCOVERY/HW_SNAPSHOT
```


### Verificar hardware snapshot

```bash
sqlite3 build/audit/vitaos-audit.db "select cpu_arch,cpu_model,ram_bytes,firmware_type,console_type,net_count,storage_count,usb_count,wifi_count from hardware_snapshot order by id desc limit 1;"
```


### Consola guiada bilingüe (hosted)

Al iniciar en hosted, VitaOS entra a la experiencia principal guiada y auditada.

Comandos base:
- `status`
- `hw`
- `audit`
- `proposals` (o `list`)
- `peers`
- `helpme`
- `emergency <texto libre>`
- `approve <id>`
- `reject <id>`
- `shutdown`

Ejemplo reproducible:

```bash
printf 'status
proposals
emergency pain and bleeding
approve P-1
reject P-2
shutdown
' | ./build/hosted/vitaos-hosted build/audit/vitaos-audit.db
```

La consola guiada emite y persiste eventos: `GUIDED_CONSOLE_SHOWN`, `MENU_OPTION_SELECTED`, `EMERGENCY_SESSION_STARTED`, `EMERGENCY_INPUT_RECEIVED`, `STRUCTURED_RESPONSE_SHOWN`.

Documentación de flujo de emergencia:
- `docs/console/emergency-flow.md`
