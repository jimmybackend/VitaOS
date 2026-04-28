# UEFI USB write verification (F1A/F1B)

Este documento define cómo validar que VitaOS escribe archivos **reales** en USB FAT bajo UEFI.

## Objetivo

Garantizar que `storage`, `notes`, `journal` y `exports` no reporten éxito si la escritura real falló.

## Requisitos de implementación

- Crear directorios en UEFI usando `EFI_FILE_MODE_CREATE` + `EFI_FILE_DIRECTORY`.
- `storage repair` crea el árbol `/vita` completo del slice F1A/F1B.
- `storage check` valida existencia real de rutas (no solo `.keep`).
- `storage write` asegura padres antes de escribir.
- `storage test` hace write+read+compare y solo reporta `VERIFIED` si coincide.
- `storage probe` hace flujo end-to-end completo.
- `storage last-error` muestra error real.
- `audit export`, `audit events`, `diagnostic`, `export index`, `selftest`, `export session` y `export jsonl` solo muestran `written` después de write+read+compare.

## Árbol esperado

```text
/vita
/vita/config
/vita/audit
/vita/notes
/vita/messages
/vita/messages/inbox
/vita/messages/outbox
/vita/messages/drafts
/vita/emergency
/vita/emergency/reports
/vita/emergency/checklists
/vita/ai
/vita/ai/prompts
/vita/ai/responses
/vita/ai/sessions
/vita/net
/vita/net/profiles
/vita/export
/vita/export/audit
/vita/export/notes
/vita/export/reports
/vita/tmp
```

## Flujo manual recomendado en USB real

```text
storage status
storage last-error
storage repair
storage check
storage probe
storage test
storage read /vita/tmp/storage-test.txt
notes list
note usb-test.txt
...escribir contenido...
.save
.exit
storage read /vita/notes/usb-test.txt
journal status
journal summary
export session
export jsonl
diagnostic
export index
selftest
shutdown
```

## Resultado esperado en PC host (después de extraer USB)

```text
/vita/tmp/storage-test.txt
/vita/notes/usb-test.txt
/vita/audit/session-journal.txt
/vita/audit/session-journal.jsonl
/vita/export/reports/last-session.txt
/vita/export/reports/diagnostic-bundle.txt
/vita/export/export-index.txt
/vita/export/reports/self-test.txt
```

## Límites de alcance

- Sin GUI/window manager.
- Sin red/Wi-Fi real.
- Sin SQLite persistente en UEFI.
- Hosted mantiene SQLite para validación.
- `tools/image/make-uefi-iso.sh` es solo para boot ISO/QEMU; no promete sembrar `/vita` completo dentro del medio ISO (requires physical USB validation).
- Para USB físico usar `tools/image/make-uefi-usb-image.sh` o ejecutar `storage repair` en el entorno writable generado por Rufus.
