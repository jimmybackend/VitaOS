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
- `docs/audit/audit-replication.md`
- `docs/audit/sqlite-contract.md`
- `docs/roadmap/F1-roadmap.md`
- `docs/network/vitanet-v0.md`
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
./build/hosted/vitaos-hosted build/audit/vitaos-audit.db 127.0.0.1:47001
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


### Consola guiada bilingüe + VitaNet v0 (hosted)

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
- `tasks`
- `tasks pending`
- `replicate`
- `shutdown`

Para probar VitaNet v0, levanta el peer de prueba y ejecuta el hosted runner con endpoint seed:

```bash
python3 tools/test/vitanet-peer.py 127.0.0.1:47001
./build/hosted/vitaos-hosted build/audit/vitaos-audit.db 127.0.0.1:47001
```

Ejemplo reproducible completo:

```bash
make smoke-audit
```

La prueba crea dos DBs:
- nodo A: `build/audit/vitaos-audit.db`
- peer B (harness): `build/audit/vitaos-peer.db`

Verificación rápida de peers:

```bash
sqlite3 build/audit/vitaos-audit.db "select peer_id,first_seen_unix,last_seen_unix,transport,trust_state,link_state from node_peer;"
```

Verificación rápida de tareas:

```bash
sqlite3 build/audit/vitaos-audit.db "select task_id,task_type,status,target_node_id,created_unix,updated_unix from node_task order by created_unix;"
sqlite3 build/audit/vitaos-peer.db "select task_id,task_type,status,origin_node_id,target_node_id from node_task order by created_unix;"
```

Eventos VitaNet esperados en auditoría: `VITANET_STARTED`, `PEER_DISCOVERED`, `PEER_CAPABILITIES_RECEIVED`, `LINK_PROPOSAL_CREATED`, `LINK_ACCEPTED/LINK_REJECTED`, `HEARTBEAT_RECEIVED`, `AUDIT_REPLICATION_ATTEMPTED`.

Documentación de flujo de emergencia:
- `docs/console/emergency-flow.md`

Documentación VitaNet v0:
- `docs/network/vitanet-v0.md`

Documentación de replicación de auditoría:
- `docs/audit/audit-replication.md`
