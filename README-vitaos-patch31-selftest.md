# VitaOS patch v31 — boot self-test command

Commit title:

```text
boot: add self-test command
```

## What this changes

Adds a bounded self-test command for the current F1A/F1B slice.

The command summarizes the runtime state in one place and writes a small report into the prepared writable tree:

```text
/vita/export/reports/self-test.txt
/vita/export/reports/self-test.jsonl
```

## Commands

```text
selftest
self-test
boot selftest
boot self-test
checkup
```

## Scope

This is a local report/check command. It does not repair, rewrite, delete or mutate audit rows. It only writes the self-test report files when storage is ready.

## Validate

```bash
bash tools/patches/verify-selftest-v31.sh
make clean
make hosted
make smoke-audit
make
make smoke
make iso
```
