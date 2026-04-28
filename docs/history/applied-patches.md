# Applied patch history (v3-v32)

Documento histórico consolidado de parches aplicados en VitaOS F1A/F1B.

**Regla de lectura:**
- **Integrated**: existe en código fuente real del repositorio.
- **Partial**: existe parte del cambio, con límites o cobertura reducida.
- **Pending**: no está completo en código real y no debe tratarse como cerrado.

## v3 ai gateway stub
- Feature/comandos: `ai status`, `ai connect`, `ai ask`, `ai disconnect`.
- Vive en: `kernel/ai_gateway.c`, `include/vita/ai_gateway.h`, `kernel/command_core.c`, `Makefile`.
- Estado esperado: stub local auditado, sin red real.
- Estado actual: **Integrated**.
- Límite F1A/F1B: sin transporte remoto real, sin Wi-Fi.

## v4 net ethernet attempt
- Feature: intento inicial de red/Ethernet con límites explícitos.
- Vive en: `kernel/net_status.c`, `kernel/net_connect.c`, docs de red.
- Estado actual: **Partial** (diagnóstico/stub, no stack completo).
- Límite: no red completa, no Wi-Fi real.

## v5 usb writable tree
- Feature: árbol base `/vita` para audit/notes/export/emergency/ai/tmp.
- Vive en: `kernel/storage.c`.
- Estado actual: **Integrated** (check/repair de árbol mínimo).
- Límite: sin VFS completo.

## v6 storage FAT writer
- Feature/comandos: `storage status`, `storage test`, `storage write`.
- Vive en: `kernel/storage.c`, `include/vita/storage.h`, `kernel/command_core.c`, `Makefile`.
- Estado actual: **Integrated** (hosted + UEFI Simple FS path).
- Límite: escritor de texto mínimo.

## v7 note editor
- Feature/comandos: `notes`, `note ...`, `notes list`.
- Vive en: `kernel/editor.c`, `include/vita/editor.h`, `kernel/command_core.c`.
- Estado actual: **Integrated**.
- Límite: consola texto, sin GUI.

## v8 storage read notes
- Feature/comandos: `storage read`, `storage cat`, `notes list`.
- Vive en: `kernel/storage.c`, `kernel/editor.c`, `kernel/command_core.c`.
- Estado actual: **Integrated**.

## v9 note editor safe edit
- Feature: append/safe-save en editor.
- Vive en: `kernel/editor.c`.
- Estado actual: **Integrated**.

## v10 storage delete/rename
- Feature: delete/rename acotado a `/vita`.
- Vive en: `kernel/storage.c`, `kernel/editor.c` (flujos de edición).
- Estado actual: **Partial** (flujo acotado, no VFS general).

## v11 session export
- Feature/comando: `export session`.
- Salida: `/vita/export/reports/last-session.txt`.
- Vive en: `kernel/session_export.c`, `include/vita/session_export.h`, `kernel/command_core.c`.
- Estado actual: **Integrated**.

## v12 session JSONL export
- Feature/comando: `export jsonl`.
- Salida: `/vita/export/reports/last-session.jsonl`.
- Vive en: `kernel/session_jsonl_export.c`, `include/vita/session_jsonl_export.h`, `kernel/command_core.c`.
- Estado actual: **Integrated**.

## v13 session journal
- Feature/comandos: `journal status/show/flush/paths/note/help`.
- Salida: `/vita/audit/session-journal.txt`, `/vita/audit/session-journal.jsonl`.
- Vive en: `kernel/session_journal.c`, `include/vita/session_journal.h`, `kernel/kmain.c`, `kernel/command_core.c`.
- Estado actual: **Integrated**.

## v14 neon terminal shell
- Feature: shell textual centrado/neon en flujo EFI cuando aplica.
- Vive en: `arch/x86_64/boot/uefi_neon_frame.c`, `arch/x86_64/boot/uefi_neon_text.c`, `arch/x86_64/boot/uefi_entry.c`, `Makefile`.
- Estado actual: **Partial** (render/fallback, sin WM).

## v15 pointer titlebar controls
- Feature: flujo de controles MIN/RESTART/POWER sin cursor GUI.
- Vive en: `arch/x86_64/boot/uefi_neon_text.c`, `kernel/power.c`.
- Estado actual: **Partial**.

## v16 safe power flow
- Feature: restart/shutdown seguro, flush journal/storage cuando aplique.
- Vive en: `kernel/power.c`, `include/vita/power.h`.
- Estado actual: **Integrated** (flujo seguro mínimo).

## v17 framebuffer neon renderer
- Feature: renderer GOP neon + fallback consola texto.
- Vive en: `arch/x86_64/boot/uefi_neon_frame.c`, `arch/x86_64/boot/uefi_entry.c`.
- Estado actual: **Integrated**.

## v18 framebuffer bitmap font renderer
- Feature: renderer texto framebuffer (font bitmap) + fallback seguro.
- Vive en: `arch/x86_64/boot/uefi_neon_text.c`.
- Estado actual: **Integrated**.

## v19 framebuffer scrollback viewport
- Feature: `PageUp`/`PageDown`, scrollback RAM sin malloc.
- Vive en: `arch/x86_64/boot/uefi_neon_text.c`.
- Estado actual: **Integrated**.

## v20 python cleanup
- Feature: no Python runtime/build dependency obligatoria.
- Vive en: `docs/tools/no-python-runtime.md`, `tools/test/smoke-audit.sh`, `README.md`.
- Estado actual: **Integrated**.

## v21 mouse wheel scroll indicator
- Feature: wheel si firmware soporta Z movement + indicador visual.
- Vive en: `arch/x86_64/boot/uefi_neon_text.c`.
- Estado actual: **Partial** (depende de firmware/hardware).

## v22 storage tree validator
- Feature/comandos: `storage tree`, `storage check`, `storage repair`, `storage validate/verify/init-tree/mkdirs`.
- Vive en: `kernel/storage.c`, `kernel/command_core.c`.
- Estado actual: **Integrated** (árbol mínimo `/vita`).

## v23 journal recovery
- Feature/comandos: `journal summary`, `journal last-session`, `journal last`, `journal recover`.
- Vive en: `kernel/session_journal.c`, `kernel/command_core.c`.
- Estado actual: **Integrated**.

## v24 audit readiness gate
- Feature/comandos: `audit status`, `audit report`, `audit gate`, `audit readiness`.
- Vive en: `kernel/command_core.c`, `kernel/audit_core.c`, `include/vita/audit.h`.
- Estado actual: **Integrated**.

## v25 diagnostic bundle
- Feature/comandos: `diagnostic`, `diag`, `export diagnostic`.
- Salida: `/vita/export/reports/diagnostic-bundle.txt`, `/vita/export/reports/diagnostic-bundle.jsonl`.
- Vive en: `kernel/command_core.c` + storage backend.
- Estado actual: **Integrated** (bundle mínimo del slice).

## v26 audit hash chain verification
- Feature/comandos: `audit verify`, `audit verify-chain`, `audit hash`, `audit hash-chain`.
- Vive en: `kernel/command_core.c`, `kernel/audit_core.c`, `include/vita/audit.h`.
- Estado actual: **Integrated** (hosted OK, UEFI restringido honesto).

## v27 audit verification export
- Feature/comandos: `audit export`, `audit verify export`, `export audit verify`.
- Salida: `/vita/export/audit/audit-verify.txt`, `/vita/export/audit/audit-verify.jsonl`.
- Vive en: `kernel/command_core.c` + storage backend.
- Estado actual: **Integrated**.

## v28 audit current session events export
- Feature/comandos: `audit events`, `audit export events`, `export audit events`.
- Salida: `/vita/export/audit/current-session-events.txt`, `/vita/export/audit/current-session-events.jsonl`.
- Vive en: `kernel/command_core.c` + storage backend.
- Estado actual: **Integrated** (export mínimo del boot actual).

## v29 hosted SQLite summary
- Feature/comandos: `audit sqlite`, `audit sqlite summary`, `audit db`, `audit db summary`.
- Vive en: `kernel/command_core.c`, `kernel/audit_core.c`, `include/vita/audit.h`.
- Estado actual: **Integrated** (hosted only, UEFI no SQLite).

## v30 export index manifest
- Feature/comandos: `export index`, `export manifest`, `export list`, `exports`, `storage export-index`.
- Salida: `/vita/export/export-index.txt`, `/vita/export/export-index.jsonl`.
- Vive en: `kernel/command_core.c` + storage backend.
- Estado actual: **Integrated**.

## v31 self-test
- Feature/comandos: `selftest`, `self-test`, `boot selftest`, `boot self-test`, `checkup`.
- Salida: `/vita/export/reports/self-test.txt`, `/vita/export/reports/self-test.jsonl`.
- Vive en: `kernel/command_core.c` + storage backend.
- Estado actual: **Integrated** (self-test mínimo del slice).

## v32 validation checklist
- Feature: checklist F1A/F1B consolidado.
- Vive en: `docs/validation/f1a-f1b-validation-checklist.md`.
- Estado actual: **Integrated**.

## v33 storage readback verification hardening
- Feature: verificación estricta write->read->compare para reportes/exportes.
- Vive en: `kernel/command_core.c`, `kernel/session_export.c`, `kernel/session_jsonl_export.c`, `kernel/storage.c`.
- Estado actual: **Integrated** (sin `written`/`VERIFIED` falso cuando falla read-back compare).
- Límite: validación USB física fuera de hosted requiere prueba en hardware real.

---

## Límites explícitos del alcance F1A/F1B

No incluido por diseño en este milestone:
- persistencia SQLite real en UEFI;
- runtime/build Python;
- GUI/window manager;
- VFS amplio;
- expansión de red/Wi-Fi real;
- cambios malloc-heavy en rutas freestanding.

## v34 automatic storage bootstrap at boot
- Feature: bootstrap automático de `/vita` al arranque (hosted + UEFI) con verificación write->read->compare.
- Vive en: `kernel/storage.c`, `include/vita/storage.h`, `kernel/kmain.c`, `kernel/session_journal.c`, `kernel/command_core.c`.
- Estado actual: **Integrated**.
- Regla crítica aplicada: no se reporta `storage verified`, `journal active` o `written/saved` sin verificación real de escritura persistente.
- Límite: validación de USB física en hardware real sigue siendo obligatoria.
