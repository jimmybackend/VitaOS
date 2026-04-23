# VitaOS con IA

Especificación base del proyecto para GitHub, desarrollo asistido por IA y codificación con Codex.

Versión: 0.1-draft
Idioma base: Español / English
Estado: Documento fundacional

> Nota de estado actual del slice (2026-04-23)
>
> Este documento se conserva como especificación fundacional completa.
> El estado práctico actual del repo ya validó un slice mínimo real de F1A/F1B con:
> - build UEFI x86_64;
> - build hosted;
> - `kmain()` integrado;
> - consola guiada mínima;
> - hardware discovery básico;
> - auditoría SQLite persistente en hosted;
> - proposal engine inicial;
> - VitaNet hosted mínimo;
> - `node_peer` persistido;
> - smoke tests de boot y auditoría.
>
> La ruta UEFI hoy debe entenderse como boot + consola + diagnóstico restringido.
> La ruta hosted es la principal para validar auditoría persistente, proposals y VitaNet mínimo.


---

## 1. Propósito del proyecto

**VitaOS con IA** es un sistema operativo desde cero, orientado inicialmente a **modo live/RAM**, con una **IA residente en el kernel**, una **bitácora obligatoria de auditoría en SQLite**, y una arquitectura de **nodos cooperativos** para ayuda, coordinación y operación en escenarios de emergencia.

La primera meta no es construir un sistema operativo completo de escritorio, sino una base técnica sólida que permita:

- arrancar en hardware real;
- detectar el hardware disponible;
- activar auditoría persistente;
- iniciar una consola textual guiada;
- permitir interacción humano–IA desde el primer boot;
- descubrir nodos cooperativos y proponer acciones útiles;
- dejar trazabilidad completa para análisis posterior.

VitaOS debe ser utilizable en modo normal por el creador para aclimatarse al sistema y desarrollar nuevas funciones, pero su diseño y prioridades están centrados en **emergencia, supervivencia, rescate, auditoría y cooperación entre nodos**.

---

## 2. Objetivos principales

### 2.1 Objetivo general

Crear un sistema operativo mínimo y extensible, con un núcleo común compilable por arquitectura, administrado por una inteligencia artificial residente, capaz de asistir al usuario en la operación del sistema, la detección de recursos, la colaboración con otros nodos y la atención de escenarios de emergencia.

### 2.2 Objetivos funcionales iniciales

1. Boot en **x86_64 UEFI** como primera plataforma estable.
2. Diseño de portabilidad desde el inicio para **ARM64**.
3. Operación **live/RAM** sin instalación obligatoria a disco.
4. Requisito de **auditoría persistente obligatoria** para pasar a modo operativo.
5. Consola inicial orientada al usuario, no solo shell técnico.
6. IA kernel-resident capaz de:
   - detectar hardware;
   - explicar capacidades disponibles;
   - proponer acciones;
   - recibir descripciones humanas de una emergencia;
   - sugerir medidas básicas de ayuda y supervivencia;
   - coordinar nodos cooperativos;
   - registrar razonamiento operativo y decisiones en auditoría.
7. Red entre nodos desde el primer boot.
8. Persistencia de auditoría con **SQLite** desde la Fase 1.

---

## 3. Principios de diseño

1. **Auditabilidad primero**  
   Si no existe un destino persistente de auditoría válido, el sistema no entra en modo operativo completo.

2. **Live by default**  
   VitaOS debe poder arrancar en RAM y operar sin instalación permanente.

3. **Text-first UX**  
   La primera interfaz será textual, guiada y bilingüe.

4. **Common kernel, thin arch layer**  
   El núcleo común debe contener la mayor parte de la lógica. Cada arquitectura solo implementa boot, MMU, interrupciones, consola y drivers mínimos.

5. **AI as kernel-resident decision core**  
   En Fase 1 la IA no será un LLM grande dentro del kernel, sino un motor residente de percepción, evaluación, propuesta y coordinación.

6. **Human-visible proposals**  
   La IA debe proponer acciones de forma legible y estructurada. El sistema registra aprobación o rechazo humano cuando aplique.

7. **Cooperative nodes**  
   VitaOS puede detectar otros nodos VitaOS, intercambiar capacidades y distribuir trabajo o replicación de auditoría.

8. **Emergency usefulness over feature breadth**  
   El sistema debe priorizar utilidad real sobre cantidad de funciones.

---

## 4. Alcance de la Fase 1

### 4.1 Resultado esperado de la Fase 1

> VitaOS F1 bootea en x86_64 UEFI, detecta hardware básico, exige auditoría SQLite persistente, inicia una consola textual guiada, levanta un núcleo de IA residente y puede descubrir nodos cooperativos por red para proponer acciones al usuario.

### 4.2 Lo que sí entra en Fase 1

- x86_64 UEFI genérico.
- Diseño preparado para ARM64.
- Consola textual guiada.
- Auditoría persistente en SQLite.
- Detección básica de hardware:
  - CPU
  - RAM
  - framebuffer/console
  - teclado
  - USB
  - almacenamiento
  - Ethernet
  - Wi-Fi (si el driver mínimo lo permite)
- Red mínima entre nodos.
- IA residente de propuestas.
- Base offline de conocimiento para:
  - primeros auxilios básicos
  - extravío
  - supervivencia básica: agua, comida, refugio, caminar hacia ayuda

### 4.3 Lo que no entra en Fase 1

- GUI completa.
- STT/TTS.
- Navegador o userland amplio.
- Control autónomo de vehículos u otros sistemas físicos críticos.
- “Soporte para todo hardware” real desde el primer commit.
- Wi-Fi avanzada si compromete el avance del núcleo.
- LoRa completa en Fase 1.
- Multiproceso complejo.
- Filesystem general de propósito amplio.

---

## 5. Arquitectura general

```text
VitaOS
├── Kernel común (C)
│   ├── memoria
│   ├── scheduler
│   ├── consola
│   ├── auditoría
│   ├── SQLite/VFS kernel
│   ├── IA residente
│   ├── propuestas
│   ├── red de nodos
│   └── knowledge packs de emergencia
├── Capa por arquitectura
│   ├── x86_64
│   └── arm64
├── Drivers mínimos
│   ├── framebuffer
│   ├── teclado
│   ├── USB
│   ├── block
│   ├── Ethernet
│   └── Wi-Fi (mínimo / posterior si aplica)
└── Protocolo VitaNet entre nodos
```

---

## 6. Modelo de estados del sistema

```text
BOOTSTRAP
  ↓
HW_DISCOVERY
  ↓
AUDIT_REQUIRED ──(audit OK)──> OPERATIONAL ──(peers found/link)──> COOPERATIVE
       │
       └──(audit missing)──> RESTRICTED_DIAGNOSTIC
```

### Estados

- **BOOTSTRAP**: boot y estructuras mínimas.
- **HW_DISCOVERY**: enumeración de hardware básico.
- **AUDIT_REQUIRED**: se busca/valida almacenamiento persistente para auditoría.
- **RESTRICTED_DIAGNOSTIC**: sin auditoría válida; solo diagnóstico y lectura.
- **OPERATIONAL**: auditoría activa, IA activa, consola lista.
- **COOPERATIVE**: operación con descubrimiento/enlace entre nodos.

---

## 7. IA residente en el kernel

### 7.1 Definición operativa

En VitaOS F1, la “IA en el kernel” es un **núcleo de decisión residente** compuesto por:

1. **Percepción**
   - estado del sistema
   - inventario de hardware
   - disponibilidad de auditoría
   - red activa
   - peers detectados
   - tipo de sesión (normal o emergencia)

2. **Evaluación**
   - riesgos
   - oportunidades
   - recursos disponibles
   - tareas posibles
   - necesidad de confirmación humana

3. **Propuesta**
   - generación de mensajes estructurados para el usuario
   - explicación de hardware útil
   - acciones sugeridas
   - solicitudes de aprobación

4. **Coordinación**
   - interacción con nodos remotos
   - elección de enlace
   - replicación de auditoría
   - reparto simple de tareas

### 7.2 Alcance de la IA en Fase 1

La IA podrá:

- explicar al usuario qué hardware tiene disponible;
- decir qué puede hacer con ese hardware;
- recibir texto libre describiendo una emergencia;
- formular preguntas críticas;
- sugerir pasos inmediatos de rescate/supervivencia;
- proponer el uso de red o peers detectados;
- proponer priorización de recursos locales;
- dejar rastro de todo en auditoría.

### 7.3 Formato de salida de la IA

Toda respuesta operativa debe estructurarse así:

1. Resumen entendido
2. Riesgos inmediatos
3. Preguntas críticas
4. Acciones inmediatas sugeridas
5. Recursos del sistema disponibles
6. Propuesta de acción del sistema
7. Estado de auditoría

---

## 8. Consola inicial

La consola inicial debe funcionar como **terminal guiada bilingüe**.

### 8.1 Pantalla de bienvenida

```text
VitaOS with AI / VitaOS con IA

Status / Estado:
- Boot: OK
- Audit SQLite: OK
- Hardware detected / Hardware detectado
- AI core: Online / IA activa

I can help you with / Puedo ayudarte con:
1. Emergency assistance / Asistencia de emergencia
2. Hardware and system status / Estado de hardware y sistema
3. Cooperative nodes / Nodos cooperativos
4. Audit review / Revisión de auditoría
5. Technical console / Consola técnica

Describe what happened or choose an option.
Describe qué pasó o elige una opción.
>
```

### 8.2 Modo emergencia

El usuario puede escribir libremente, por ejemplo:

```text
there are two lost people in the woods, no internet, night is coming
```

o bien:

```text
hay una persona extraviada, tengo agua limitada y necesito saber hacia dónde moverme
```

La IA debe:

- detectar el tema;
- clasificar la situación;
- preguntar por variables faltantes;
- proponer pasos concretos;
- ofrecer acciones del sistema;
- registrar todo.

### 8.3 Comandos base

- `status`
- `hw`
- `audit`
- `peers`
- `proposals`
- `emergency`
- `helpme`
- `approve <id>`
- `reject <id>`
- `export`
- `shutdown`

---

## 9. Auditoría persistente

### 9.1 Regla central

**Sin auditoría persistente, VitaOS no debe pasar a modo operativo completo.**

### 9.2 Medio persistente de Fase 1

Se prioriza una **misma USB live** con dos áreas:

1. partición de arranque live;
2. partición writable dedicada a auditoría.

### 9.3 Estrategia de persistencia

Orden de preferencia:

1. SQLite local en partición writable del mismo USB;
2. replicación secundaria a peer si existe;
3. exportación posterior a JSONL/SQL dump.

### 9.4 Requisitos del subsistema de auditoría

- append-mostly;
- hash chain por evento;
- secuencia monotónica;
- identificación de boot;
- identificación de nodo;
- timestamps monotónicos y RTC cuando existan;
- eventos de sistema, IA, humano y red;
- export y validación.

---

## 10. Esquema SQLite inicial

```sql
CREATE TABLE boot_session (
    boot_id TEXT PRIMARY KEY,
    node_id TEXT NOT NULL,
    arch TEXT NOT NULL,
    boot_unix INTEGER,
    boot_monotonic_ns INTEGER,
    mode TEXT NOT NULL,
    audit_state TEXT NOT NULL,
    language_mode TEXT NOT NULL DEFAULT 'es,en'
);

CREATE TABLE hardware_snapshot (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    boot_id TEXT NOT NULL,
    cpu_arch TEXT,
    cpu_model TEXT,
    ram_bytes INTEGER,
    firmware_type TEXT,
    console_type TEXT,
    net_count INTEGER,
    storage_count INTEGER,
    usb_count INTEGER,
    wifi_count INTEGER DEFAULT 0,
    detected_at_unix INTEGER NOT NULL,
    FOREIGN KEY (boot_id) REFERENCES boot_session(boot_id)
);

CREATE TABLE audit_event (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    boot_id TEXT NOT NULL,
    event_seq INTEGER NOT NULL,
    event_type TEXT NOT NULL,
    severity TEXT NOT NULL,
    actor_type TEXT NOT NULL,
    actor_id TEXT,
    target_type TEXT,
    target_id TEXT,
    summary TEXT NOT NULL,
    details_json TEXT,
    monotonic_ns INTEGER NOT NULL,
    rtc_unix INTEGER,
    prev_hash TEXT,
    event_hash TEXT NOT NULL,
    FOREIGN KEY (boot_id) REFERENCES boot_session(boot_id)
);

CREATE TABLE ai_proposal (
    proposal_id TEXT PRIMARY KEY,
    boot_id TEXT NOT NULL,
    created_unix INTEGER NOT NULL,
    proposal_type TEXT NOT NULL,
    summary TEXT NOT NULL,
    rationale TEXT NOT NULL,
    risk_level TEXT NOT NULL,
    benefit_level TEXT NOT NULL,
    requires_human_confirmation INTEGER NOT NULL,
    status TEXT NOT NULL,
    FOREIGN KEY (boot_id) REFERENCES boot_session(boot_id)
);

CREATE TABLE human_response (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    proposal_id TEXT NOT NULL,
    boot_id TEXT NOT NULL,
    response TEXT NOT NULL,
    operator_key TEXT DEFAULT 's/n',
    response_unix INTEGER NOT NULL,
    FOREIGN KEY (proposal_id) REFERENCES ai_proposal(proposal_id),
    FOREIGN KEY (boot_id) REFERENCES boot_session(boot_id)
);

CREATE TABLE node_peer (
    peer_id TEXT PRIMARY KEY,
    first_seen_unix INTEGER NOT NULL,
    last_seen_unix INTEGER NOT NULL,
    transport TEXT NOT NULL,
    capabilities_json TEXT NOT NULL,
    trust_state TEXT NOT NULL,
    link_state TEXT NOT NULL DEFAULT 'discovered'
);

CREATE TABLE node_task (
    task_id TEXT PRIMARY KEY,
    origin_node_id TEXT NOT NULL,
    target_node_id TEXT NOT NULL,
    task_type TEXT NOT NULL,
    status TEXT NOT NULL,
    created_unix INTEGER NOT NULL,
    updated_unix INTEGER NOT NULL,
    payload_json TEXT
);

CREATE TABLE knowledge_pack (
    pack_id TEXT PRIMARY KEY,
    name TEXT NOT NULL,
    version TEXT NOT NULL,
    lang TEXT NOT NULL,
    topic TEXT NOT NULL,
    source_hash TEXT NOT NULL,
    loaded_unix INTEGER NOT NULL
);
```

---

## 11. Hash chain de eventos

Cada evento de auditoría debe calcular:

```text
event_hash = HASH(
  boot_id || event_seq || event_type || severity || actor_type || summary || details_json || monotonic_ns || prev_hash
)
```

Esto permite verificar integridad aun cuando la base SQLite sea la capa de almacenamiento principal.

---

## 12. Nodos cooperativos

### 12.1 Modelo

VitaOS debe poder descubrir otros nodos VitaOS por red y mantener una tabla local de peers.

### 12.2 Objetivos de los nodos cooperativos

- replicar auditoría;
- compartir capacidad de cómputo;
- actuar como espejo o respaldo;
- permitir coordinación operativa;
- mostrar al humano qué hardware adicional existe y qué podría hacerse con él.

### 12.3 Transportes iniciales

Fase 1 prioriza:

1. Ethernet
2. Wi-Fi

### 12.4 Protocolo inicial (VitaNet v0)

Mensajes básicos:

- `HELLO`
- `CAPS`
- `HEARTBEAT`
- `PROPOSAL`
- `LINK_ACCEPT`
- `LINK_REJECT`
- `TASK_ASSIGN`
- `TASK_DONE`
- `AUDIT_REPL`

### 12.5 Ejemplo de anuncio de capacidades

```json
{
  "node_id": "A1F2",
  "arch": "x86_64",
  "ram_mb": 8192,
  "transports": ["ethernet", "wifi"],
  "roles": ["audit", "coordination"],
  "audit_ready": true,
  "ai_level": "kernel-core"
}
```

---

## 13. Knowledge packs offline

La IA de emergencia no debe depender solo de texto generativo. Debe apoyarse en **paquetes offline auditables**.

### Temas iniciales

1. Primeros auxilios básicos
2. Extravío / orientación básica
3. Agua segura
4. Comida de emergencia y conservación
5. Refugio básico
6. Caminar hacia ayuda con menor riesgo

### Requisitos

- versión y hash por paquete;
- idioma `es` y `en`;
- fuente rastreable;
- carga registrada en auditoría.

---

## 14. Soporte bilingüe

VitaOS F1 será **bilingüe español/inglés**.

### Reglas

- interfaz principal: ambas lenguas visibles;
- comandos: preferentemente en inglés corto (`status`, `audit`, `peers`) pero aceptando equivalentes en español más adelante;
- respuestas de la IA: en el idioma detectado del usuario, con capacidad de alternancia futura.

---

## 15. Estructura inicial del repositorio

```text
vitaos/
├── README.md
├── AGENTS.md
├── docs/
│   ├── VitaOS-SPEC.md
│   ├── roadmap/
│   ├── architecture/
│   └── audit/
├── kernel/
│   ├── kmain.c
│   ├── console.c
│   ├── panic.c
│   ├── memory.c
│   ├── scheduler.c
│   ├── ai_core.c
│   ├── proposal.c
│   ├── audit_core.c
│   ├── sqlite_vfs.c
│   ├── node_core.c
│   └── emergency_core.c
├── arch/
│   ├── x86_64/
│   │   ├── boot/
│   │   ├── mmu/
│   │   ├── irq/
│   │   └── timer/
│   └── arm64/
│       ├── boot/
│       ├── mmu/
│       ├── irq/
│       └── timer/
├── drivers/
│   ├── framebuffer/
│   ├── keyboard/
│   ├── block/
│   ├── usb/
│   ├── ethernet/
│   └── wifi/
├── proto/
│   └── vitanet/
├── knowledge/
│   ├── es/
│   └── en/
├── schema/
│   └── audit.sql
├── tools/
│   ├── build/
│   ├── image/
│   └── test/
└── tests/
```

---

## 16. Orden de implementación recomendado

### F1A — x86_64 base

1. Boot UEFI x86_64
2. Consola básica
3. Memoria básica
4. Timer/clock
5. Detección de CPU/RAM
6. Detección de USB/almacenamiento
7. SQLite de auditoría en partición writable
8. `boot_session` + `audit_event`
9. IA mínima con propuestas de sistema
10. consola guiada inicial

### F1B — red y cooperación

1. Ethernet básica
2. descubrimiento VitaNet
3. `node_peer`
4. `node_task`
5. propuestas de vinculación entre nodos
6. replicación básica de auditoría

### F1C — ARM64

1. boot ARM64
2. port del núcleo común
3. validación del flujo de auditoría
4. validación de consola guiada
5. validación de nodos cooperativos mixtos

---

## 17. Recomendación de entorno de desarrollo

### Sistema base recomendado

**Ubuntu 24.04 LTS** en EC2 para el host de compilación principal.

### Motivo

- toolchain madura para C, cross-compilers y QEMU;
- buena experiencia en nube y automatización;
- integración actual en AWS;
- base estable para build reproducible.

### Instancia EC2 recomendada para iniciar

**c7i.xlarge** como instancia principal de compilación.

### Perfil sugerido

- 4 vCPU
- 8 GiB RAM
- 100 GiB gp3
- Ubuntu 24.04 LTS

### Uso previsto

- builds del kernel;
- cross-compile a ARM64;
- QEMU para arranque y pruebas iniciales;
- documentación y repositorio GitHub;
- trabajo asistido por Codex.

### Instancia opcional posterior para ARM64 nativo

**c7g.large** o superior, cuando convenga validar builds y pruebas nativas ARM64.

---

## 18. Reglas de codificación para Codex / IA asistente

1. No inventar features fuera del alcance del milestone.
2. No introducir C++ en el kernel inicial.
3. Mantener C claro, freestanding y modular.
4. Todo cambio importante debe considerar impacto en auditoría.
5. Si se agrega una capacidad nueva, debe decidirse si requiere:
   - evento de auditoría,
   - propuesta de IA,
   - confirmación humana.
6. El camino feliz y los errores deben dejar traza en SQLite.
7. Mantener las estructuras y enums sincronizados con `schema/audit.sql`.
8. Priorizar x86_64 UEFI antes de ampliar alcance.
9. Las respuestas de la IA operativa deben usar formato estructurado.
10. Ningún módulo debe asumir que habrá instalación en disco.

---

## 19. Prompt de referencia para Codex

```text
This repository contains VitaOS with AI, a from-scratch operating system project.
Primary goals for the current milestone:
- x86_64 UEFI boot
- live/RAM operation
- mandatory persistent audit in SQLite
- guided bilingual text console
- kernel-resident AI proposal engine
- basic cooperative node discovery over Ethernet/Wi-Fi

Constraints:
- C for common kernel code
- minimal assembly only where necessary
- no C++ in the initial kernel
- no feature creep outside current milestone
- every important action must be auditable
- SQLite schema is authoritative
- code should be modular, readable, and milestone-focused

Before generating code:
1. identify the milestone and submodule;
2. preserve the audit model;
3. avoid assuming unavailable drivers or services;
4. explain design decisions briefly;
5. produce complete copy-pasteable code when requested.
```

---

## 20. Siguientes entregables útiles

1. `README.md` corto para GitHub
2. `AGENTS.md` específico para IA/Codex
3. `schema/audit.sql`
4. `docs/architecture/boot-flow.md`
5. `docs/architecture/ai-core.md`
6. `docs/audit/audit-model.md`
7. `docs/network/vitanet-v0.md`
8. `docs/console/emergency-flow.md`

---

## 21. Resumen ejecutivo

VitaOS con IA es un sistema operativo desde cero, live-first, audit-first, text-first, con una IA residente que explica hardware, propone acciones, ayuda en emergencias y coordina nodos cooperativos. La Fase 1 debe centrarse en una base sólida: boot, auditoría SQLite, consola guiada, detección de hardware e interacción inicial entre nodos.

Este documento es la referencia fundacional para el repositorio y para la colaboración con asistentes de IA durante el desarrollo.
