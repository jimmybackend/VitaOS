# UEFI USB write verification (F1A/F1B)

Este documento define cómo validar que VitaOS escribe archivos **reales** en USB FAT bajo UEFI.

## Objetivo

Garantizar que `storage`, `notes`, `journal` y `exports` no reporten éxito si la escritura real falló.

## Requisitos de implementación

- VitaOS ejecuta **bootstrap automático de storage en boot** (`storage_bootstrap_persistent_tree()`).
- En UEFI, VitaOS intenta seleccionar un `EFI Simple File System` realmente escribible:
  - enumera candidatos FAT;
  - intenta write->read->compare de marcador en cada candidato;
  - selecciona backend solo si la verificación pasa.
- El bootstrap crea el árbol `/vita` completo usando la lista única de directorios esperados.
- El bootstrap ejecuta `storage_tree_repair()` + `storage_tree_check()` + marcador `write -> read -> compare` en:
  - `/vita/tmp/boot-storage-verify.txt`
- `storage repair` y `storage check` siguen existiendo, pero son **recuperación/diagnóstico manual**, no precondición del boot.
- `journal` solo se marca `active` cuando el flush inicial pasa `write -> read -> compare`.
- `storage last-error` muestra error real.
- `audit export`, `audit events`, `diagnostic`, `export index`, `selftest`, `export session` y `export jsonl` solo muestran `written` después de write+read+compare.

## Árbol esperado tras boot exitoso

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
/vita/tmp/boot-storage-verify.txt
/vita/audit/session-journal.txt
/vita/audit/session-journal.jsonl
```

## Flujo manual recomendado en USB real (sin `storage repair` al inicio)

```text
storage status
storage last-error
storage check
journal status
journal summary
note usb-test.txt
...escribir contenido...
.save
.exit
storage read /vita/notes/usb-test.txt
export session
export jsonl
diagnostic
export index
selftest
shutdown
```

## Rufus/ISO: criterio honesto

- VitaOS debe intentar crear `/vita` automáticamente al boot en el filesystem writable que exponga UEFI.
- Si Rufus deja el medio en modo read-only o firmware no expone Simple File System writable, VitaOS debe fallar de forma visible y honesta:
  - `storage: unavailable or unverified`
  - `storage bootstrap: failed`
  - `journal: inactive`
  - `audit persistence: restricted diagnostic`
- No depende de crear carpetas manualmente en Windows.
- UEFI no promete SQLite persistente; en ese modo la persistencia mínima es `journal/jsonl` cuando el FS es escribible y verificado.

## Resultado esperado en PC host (después de extraer USB)

```text
/vita/tmp/boot-storage-verify.txt
/vita/notes/usb-test.txt
/vita/audit/session-journal.txt
/vita/audit/session-journal.jsonl
/vita/export/reports/last-session.txt
/vita/export/reports/last-session.jsonl
/vita/export/reports/diagnostic-bundle.txt
/vita/export/export-index.txt
/vita/export/reports/self-test.txt
```

## Límites de alcance

- Sin GUI/window manager.
- Sin red/Wi-Fi real.
- Sin SQLite persistente en UEFI.
- Hosted mantiene SQLite para validación.
- `tools/image/make-uefi-iso.sh` es solo para boot ISO/QEMU; la persistencia real depende de que el USB final sea writable.
- La validación final sigue siendo física: retirar USB y verificar archivos desde otra computadora.
