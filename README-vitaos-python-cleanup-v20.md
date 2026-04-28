# VitaOS patch v20 — Python cleanup

Commit title:

```text
tools: replace patch helpers with shell scripts
```

## What this changes

This patch keeps Python out of VitaOS runtime and build paths.

It does three safe things:

1. Removes the stale `python3` hosted requirement from `README.md`.
2. Adds `docs/tools/no-python-runtime.md` explaining the classification:
   - C/freestanding runtime: no Python.
   - Make/build/smoke paths: no Python.
   - Old `.py` files under `tools/patches/`: legacy development patch helpers, removed by this patch.
3. Adds `tools/patches/verify-no-python-runtime.sh`, a Bash-only checker for runtime/build Python references.

## Apply

From the repository root:

```bash
unzip -o vitaos-python-cleanup-and-mouse-scroll.zip -d .
bash tools/patches/apply-python-cleanup-v20.sh
```

## Validate

```bash
bash tools/patches/verify-no-python-runtime.sh
make clean
make hosted
make smoke-audit
make
make smoke
make iso
```

## Scope

Included:

- no Python runtime dependency;
- no Python build dependency;
- removal of legacy `.py` patch helpers under `tools/patches/`;
- Bash-only verification helper.

Not included:

- Python inside `BOOTX64.EFI`;
- new OS dependencies;
- GUI/window manager;
- network/Wi-Fi expansion.
