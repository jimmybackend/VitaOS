# VitaOS con IA

VitaOS es un **sistema operativo desde cero** (from-scratch), orientado a operación **live/RAM**, con enfoque **audit-first** y **text-first**.

Está diseñado para uso en USB booteable y escenarios de emergencia donde importa más la trazabilidad, la honestidad operativa y la utilidad inmediata que el scope amplio.

> Estado de IA real: hoy VitaOS está preparado para integración de IA (gateway/stub/flujo local de emergencia), pero **no** debe afirmarse integración real con AWS Bedrock ni IA local completa en producción.

> Estado de red real: hoy VitaOS **no** debe afirmarse como red/Wi‑Fi real completa en hardware UEFI.

---

## Estado actual validado

Estado real validado en el slice actual:

- Boot en **UEFI** y **hosted**.
- Persistencia real en USB writable.
- Creación automática del árbol `/vita` durante boot.
- Verificación de storage con flujo **write -> read -> compare**.
- Flujo con USB real creada con Rufus validado.
- Editor seguro operativo.
- Historial persistente por sesión operativo.
- Reportes TXT/JSONL operativos.
- Validadores automáticos operativos.

Notas operativas importantes:

- No se requiere `storage repair` manual para preparar `/vita`.
- El boot puede tardar algunos segundos mientras detecta y verifica filesystem writable.
- Se crean archivos reales sobre USB cuando el backend writable está disponible.

---

## Modos de ejecución

### Hosted (ruta principal de validación rápida)

- Usa filesystem local (`build/storage`) como backend de pruebas.
- Puede usar SQLite hosted para auditoría donde aplica el flujo hosted.
- El autocompletado puede listar notas reales desde `build/storage/vita/notes`.
- Es la vía recomendada para iteración rápida y validadores automatizados.

### UEFI (hardware/USB real)

- Arranca en hardware/USB.
- Crea/verifica `/vita` en storage writable.
- No debe prometer SQLite persistente UEFI completa todavía.
- No debe prometer red/Wi‑Fi real completa todavía.
- Debe usar fallback visual seguro si ANSI no es soportado.
- Puede operar en modo diagnóstico restringido cuando no existe auditoría persistente completa.

---

## Árbol `/vita` persistente

Estructura base esperada:

```text
/vita/
/vita/tmp/
/vita/audit/
/vita/audit/sessions/
/vita/notes/
/vita/export/
/vita/export/reports/
```

Propósito de cada carpeta:

- `/vita/`: raíz persistente de VitaOS en storage writable.
- `/vita/tmp/`: verificaciones temporales de boot/storage.
- `/vita/audit/`: journal actual y artefactos de auditoría de sesión.
- `/vita/audit/sessions/`: historial por sesión (sin sobreescribir sesiones previas).
- `/vita/notes/`: notas del usuario y pruebas directas de persistencia.
- `/vita/export/`: exportaciones de sesión/diagnóstico.
- `/vita/export/reports/`: reportes legibles (TXT) y técnicos (JSONL).

---

## Archivos persistentes esperados

```text
/vita/tmp/boot-storage-verify.txt
/vita/audit/session-journal.txt
/vita/audit/session-journal.jsonl
/vita/audit/sessions/session-*.txt
/vita/audit/sessions/session-*.jsonl
/vita/notes/usb-test.txt
/vita/export/reports/last-session.txt
/vita/export/reports/last-session.jsonl
/vita/export/reports/diagnostic-bundle.txt
/vita/export/reports/diagnostic-bundle.jsonl
/vita/export/export-index.txt
/vita/export/reports/self-test.txt
/vita/export/reports/self-test.jsonl
```

---

## Editor seguro

Comandos dentro del editor:

- `.save`
  - Guarda y continúa editando.
  - Usa verificación `write -> read -> compare`.
  - No reporta guardado exitoso si falla la verificación.

- `.wq`
  - Guarda y sale.
  - Solo sale como guardado si `write/read/compare` fue exitoso.

- `.exit`
  - Sale sin guardar cambios pendientes.
  - No se persiste como texto en la nota.

- `.quit`
  - Alias de salida sin guardar.

- `.help`
  - Muestra ayuda del editor.

Reglas clave:

- `.save/.exit/.wq/.quit/.help` son comandos de control y **no** se escriben dentro de la nota.
- El editor tiene marco visual tipo hoja/nano.
- El color cyan/celeste del texto de edición es visual (principalmente hosted).
- No se guardan ANSI ni bordes visuales dentro de la nota persistida.

---

## Historial persistente de sesión

Ruta:

- `/vita/audit/sessions/`

Ejemplos:

- `/vita/audit/sessions/session-000001.txt`
- `/vita/audit/sessions/session-000001.jsonl`

Comportamiento:

- No se sobrescriben sesiones anteriores.
- Se guarda input del usuario.
- Se guarda output del sistema.
- Se guardan eventos del editor.
- JSONL conserva claves técnicas en inglés para consumo estable.
- TXT prioriza legibilidad humana.

Campos JSONL técnicos importantes:

- `session_id`
- `boot_id`
- `node_id`
- `host_id`
- `arch`
- `firmware`
- `boot_mode`
- `cpu_model`
- `ram_bytes`
- `storage_backend`
- `storage_state`
- `event_type`
- `actor`
- `text`
- `seq`

---

## Autocompletado con Tab

Estado actual:

- Hosted-first.
- Completa comandos conocidos.
- Completa rutas conocidas.
- Puede listar notas reales en hosted.
- Si hay una coincidencia, completa directamente.
- Si hay múltiples coincidencias, lista opciones.
- No ejecuta automáticamente sin Enter.
- No se activa dentro del editor.
- En UEFI, disponibilidad puede depender del backend activo.

Comandos mínimos documentados:

- `status`
- `hw`
- `audit`
- `audit status`
- `storage status`
- `storage check`
- `storage last-error`
- `journal status`
- `journal summary`
- `note`
- `notes list`
- `edit`
- `export session`
- `export jsonl`
- `diagnostic`
- `export index`
- `selftest`
- `shutdown`

---

## Colores y UX visual

### Hosted

- ANSI habilitado.
- Mensajería en español: verde.
- Errores: rojo.
- OK: verde.
- Warnings: amarillo.
- Input de usuario: rojo.
- Texto del editor: cyan.

### UEFI

- Fallback seguro.
- Sin ANSI roto.
- Texto normal si no hay soporte real.

Persistencia y sanitización:

- No se guardan ANSI en notas.
- No se guardan ANSI en transcript/journal.
- No se guardan ANSI en reportes exportados.

---

## Reportes

- TXT: español primero, lectura humana.
- JSONL: claves técnicas estables en inglés.

Rutas principales:

- `/vita/export/reports/last-session.txt`
- `/vita/export/reports/last-session.jsonl`
- `/vita/export/reports/diagnostic-bundle.txt`
- `/vita/export/reports/diagnostic-bundle.jsonl`
- `/vita/export/reports/self-test.txt`
- `/vita/export/reports/self-test.jsonl`
- `/vita/export/export-index.txt`

---


## VitaIR-Tri: internal ternary representation

VitaOS define **VitaIR-Tri** como una representación interna textual y auditable para expresar *claims* de estado con semántica ternaria (`-1`, `0`, `+1`).

¿Por qué existe?

- Para unificar estados operativos de forma simple y verificable por humanos.
- Para preparar consumo futuro en status/audit/selftest/export sin romper el flujo actual.
- Para mantener separación clara entre **estado** (ternario) y **severidad** (`info/warn/error/critical`).

Límites explícitos:

- **No** reemplaza SQLite ni el roadmap de auditoría SQLite.
- **No** reemplaza el audit log TXT/JSONL actual.
- **No** es BitNet, no es un LLM y no implica IA local completa implementada.

Su adopción será gradual y honesta, empezando por documentación y posterior uso incremental en status/audit/selftest/export.

## Validadores y checks

Comandos de validación usados en el flujo actual:

```bash
make clean
make hosted
make smoke-audit
make
./tools/test/validate-console-editor-history.sh
./tools/test/validate-storage-persistence.sh
./tools/test/validate-vitaos.sh
make iso
make smoke
```

Qué valida cada script clave:

- `validate-console-editor-history.sh`:
  - editor;
  - nota limpia;
  - `session-*.txt/jsonl`;
  - eventos `user_input`, `system_output`, `editor_save`;
  - ausencia de ANSI persistido.

- `validate-storage-persistence.sh`:
  - persistencia storage USB/hosted;
  - presencia de reportes esperados.

- `validate-vitaos.sh`:
  - validación general del flujo VitaOS actual.

---

## Limitaciones reales

No implementado todavía:

- SQLite persistente completa en UEFI.
- Red/Wi‑Fi real completa.
- Integración real con AWS Bedrock.
- IA local real completa.
- GUI.
- Framebuffer.
- VFS amplio.
- Userland amplio.

Limitaciones de entorno (no bug de código):

- `make iso` requiere `xorriso`.
- `make smoke` requiere `qemu-system-x86_64` + OVMF.

Si faltan esas dependencias, reportar como limitación del entorno.

---

## Dependencias

### Build básico

- `clang`
- `lld`
- `make`
- `sqlite3` / `libsqlite3` (hosted)
- `coreutils`

### ISO

- `xorriso`

### Smoke QEMU/UEFI

- `qemu-system-x86_64`
- OVMF

---

## Quick start

```bash
make clean
make hosted
./build/hosted/vitaos-hosted
```

Prueba rápida de editor:

```text
edit /vita/notes/test.txt
hola desde VitaOS
.save
.exit
storage read /vita/notes/test.txt
```

Prueba rápida de reportes:

```text
storage status
journal summary
export session
export jsonl
diagnostic
selftest
```

---

## Última validación conocida

- `make clean`: PASS
- `make hosted`: PASS
- `make smoke-audit`: PASS
- `make`: PASS
- `validate-console-editor-history`: PASS
- `validate-storage-persistence`: PASS
- `validate-vitaos`: PASS
- `make iso`: limitación real por `xorriso` faltante
- `make smoke`: limitación real por `qemu/ovmf` faltante

---

## Historial reciente de PRs (referencia)

- PR 17: editor seguro
- PR 18: historial persistente por sesión
- PR 19: protección anti-recursión `transcript_error`
- PR 20: metadatos reales de sesión/equipo
- PR 21: Tab autocomplete
- PR 22: bounds fix autocomplete
- PR 23: colores bilingües
- PR 24: reportes español primero
- PR 25: validador consola/editor/historial
- PR 26: prompt/input visual
- PR 27: editor visual tipo hoja/nano

---

## Honestidad técnica (regla explícita)

VitaOS mantiene `audit-first` como regla central.

- No afirmar capacidades no implementadas.
- No afirmar IA remota/local real cuando solo hay stubs o preparación.
- No afirmar SQLite persistente completa en UEFI cuando no existe aún.
- Si no hay auditoría persistente válida para modo completo, operar y comunicar modo restringido/diagnóstico.

