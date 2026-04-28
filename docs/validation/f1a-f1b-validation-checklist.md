# VitaOS F1A/F1B validation checklist

Checklist operativo para validar el slice actual F1A/F1B manteniendo VitaOS text-first, audit-first y sin dependencias Python para runtime/build.

## 1) Preflight

```bash
git status
git log --oneline -8
```

Esperado: árbol limpio antes de validar.

## 2) Verificación no-Python (obligatoria)

```bash
find . \
  -path './.git' -prune -o \
  -path './build' -prune -o \
  -name '*.py' -print
```

Esperado: sin salida.

```bash
grep -RInE '(^#!.*python|python3|[^A-Za-z0-9_]python[^A-Za-z0-9_])' \
  Makefile arch include kernel drivers proto tools/build tools/image tools/test tests schema docs README.md
```

Esperado: solo menciones documentales que indiquen que Python **NO** es dependencia del runtime/build.

Referencia: `docs/tools/no-python-runtime.md`.

## 3) Build y smoke

```bash
make clean
make hosted
make smoke-audit
make
make smoke
make iso
```

Esperado:
- build hosted OK;
- smoke-audit OK;
- build EFI OK;
- smoke boot OK;
- ISO generado.

## 4) Flujo manual hosted

```bash
printf '%s\n' \
  'storage status' \
  'storage last-error' \
  'storage check' \
  'journal status' \
  'journal summary' \
  'note usb-test.txt' \
  'primera linea' \
  '.save' \
  '.exit' \
  'notes list' \
  'storage read /vita/notes/usb-test.txt' \
  'export session' \
  'export jsonl' \
  'diagnostic' \
  'export index' \
  'selftest' \
  'shutdown' \
  | ./build/hosted/vitaos-hosted
```

Esperado:
- el boot prepara `/vita` automáticamente sin `storage repair`;
- `boot-storage-verify.txt` existe;
- journal/exportes/notas escriben con verificación de lectura;
- no se reclama `written/saved/active` cuando falla read-back compare.
- en UEFI, SQLite se reporta como hosted-only y la persistencia mínima validable es journal/jsonl.

## 5) Flujo manual UEFI (hardware real)

En consola VitaOS UEFI ejecutar:

```text
storage repair
storage check
selftest
audit readiness
audit verify
audit export
audit events
diagnostic
export index
journal summary
shutdown
```

Esperado:
- consola local usable;
- storage mínimo `/vita` preparado;
- export/reportes locales escritos cuando FAT está disponible;
- UEFI mantiene diagnóstico restringido hasta persistencia de auditoría real.

## 6) Criterio de aceptación

Se acepta F1A/F1B cuando:
- no hay `.py` en el árbol del repo (excluyendo `.git`/`build`);
- no hay dependencia Python en build/smoke obligatorios;
- `make clean && make hosted && make smoke-audit && make && make smoke && make iso` pasa;
- hosted valida SQLite/audit;
- UEFI no afirma auditoría persistente completa sin backend real.
