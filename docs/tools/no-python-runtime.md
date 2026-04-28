# VitaOS: no Python runtime/build dependency

VitaOS F1A/F1B **no requiere Python** para su runtime del sistema operativo ni para el flujo de build obligatorio.

## Regla

- No Python en BOOTX64.EFI.
- No Python en kernel/freestanding.
- No Python como requisito de `make`, `make hosted`, `make smoke-audit`, `make smoke`, `make iso`.

## Verificación rápida

```bash
find . \
  -path './.git' -prune -o \
  -path './build' -prune -o \
  -name '*.py' -print
```

El resultado esperado es vacío.

```bash
grep -RInE '(^#!.*python|python3|[^A-Za-z0-9_]python[^A-Za-z0-9_])' \
  Makefile arch include kernel drivers proto tools/build tools/image tools/test tests schema docs README.md
```

Cualquier mención encontrada debe ser documental y explícita en que Python **NO** es parte del runtime/build requerido.
