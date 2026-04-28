# VitaOS F1A/F1B validation checklist

Patch: `docs: add F1A/F1B validation checklist`

This checklist validates the current VitaOS F1A/F1B base after the storage, journal, audit, export, diagnostic and self-test hardening patches.

The goal is to prove that the system is still:

- text-first;
- audit-first;
- storage-bounded under `/vita/`;
- honest about UEFI restricted diagnostic mode;
- free of Python runtime/build dependency for the OS path;
- free of GUI/window-manager expansion;
- ready for the next design phase: UEFI persistent audit backend.

## 1. Clean tree preflight

From the repository root:

```bash
git status
git log --oneline -8
```

Expected:

- working tree clean before applying new work;
- latest commits include recent patches such as self-test, export manifest, SQLite summary, audit events export, audit verification export.

## 2. Python cleanup verification

Run when the helper exists:

```bash
bash tools/patches/verify-no-python-runtime.sh
```

Expected:

- no `.py` files in the repository tree;
- no Python references in runtime/build paths.

If this helper is unavailable in an older tree, verify manually:

```bash
find . \
  -path './.git' -prune -o \
  -path './build' -prune -o \
  -name '*.py' -print
```

Expected:

- no output.

## 3. Patch-level verifier sweep

Run the available patch verifiers. It is OK if an older verifier is absent, but every verifier present in `tools/patches/` should pass.

```bash
for f in tools/patches/verify-*.sh; do
  echo "== $f =="
  bash "$f"
done
```

Expected:

- every present verifier exits successfully;
- no unexpected Python reference;
- no missing symbols for committed patches.

## 4. Build validation

Run the full build set:

```bash
make clean
make hosted
make smoke-audit
make
make smoke
make iso
```

Expected:

- hosted binary builds;
- smoke-audit passes;
- EFI binary builds;
- smoke boot passes;
- ISO is produced.

Expected paths:

```text
build/hosted/vitaos-hosted
build/efi/EFI/BOOT/BOOTX64.EFI
build/iso/vitaos-uefi-live.iso
```

## 5. Hosted interactive validation

Run hosted mode with a scripted command flow:

```bash
printf '%s\n' \
  'storage repair' \
  'storage check' \
  'journal status' \
  'journal summary' \
  'audit readiness' \
  'audit verify' \
  'audit export' \
  'audit events' \
  'audit sqlite' \
  'diagnostic' \
  'export index' \
  'selftest' \
  'shutdown' \
  | ./build/hosted/vitaos-hosted
```

Expected:

- storage tree is repaired or already valid;
- journal is active;
- audit readiness is operational in hosted;
- audit hash-chain verification reports OK;
- audit exports are written;
- diagnostic bundle is written;
- export index is written;
- self-test writes its report.

## 6. Hosted output files

After the hosted run, check the generated files:

```bash
ls -R build/storage/vita
```

Expected known files include some or all of:

```text
build/storage/vita/audit/session-journal.txt
build/storage/vita/audit/session-journal.jsonl
build/storage/vita/export/audit/audit-verify.txt
build/storage/vita/export/audit/audit-verify.jsonl
build/storage/vita/export/audit/current-session-events.txt
build/storage/vita/export/audit/current-session-events.jsonl
build/storage/vita/export/reports/diagnostic-bundle.txt
build/storage/vita/export/reports/diagnostic-bundle.jsonl
build/storage/vita/export/reports/self-test.txt
build/storage/vita/export/reports/self-test.jsonl
build/storage/vita/export/export-index.txt
build/storage/vita/export/export-index.jsonl
```

Quick content checks:

```bash
grep -R "VitaOS" build/storage/vita/export build/storage/vita/audit || true
grep -R "PASS\|WARN\|FAIL" build/storage/vita/export/reports/self-test.txt || true
```

Expected:

- diagnostic and self-test reports contain readable status lines;
- export index contains present/missing entries for known export paths.

## 7. Hosted SQLite audit checks

Inspect SQLite manually when available:

```bash
sqlite3 build/audit/vitaos-audit.db "select count(*) from boot_session;"
sqlite3 build/audit/vitaos-audit.db "select count(*) from audit_event;"
sqlite3 build/audit/vitaos-audit.db "select event_seq,event_type,prev_hash,event_hash from audit_event order by id limit 10;"
```

Expected:

- at least one `boot_session`;
- audit events exist;
- `event_seq` increases in the current boot session.

The console command should also work:

```bash
printf '%s\n' 'audit sqlite' 'shutdown' | ./build/hosted/vitaos-hosted
```

Expected:

- hosted SQLite summary shows boot/session/event counts.

## 8. UEFI manual validation

Build and boot the ISO or USB image:

```bash
make iso
```

Inside VitaOS UEFI, run:

```text
storage repair
storage check
journal status
journal summary
audit readiness
audit verify
audit export
audit events
diagnostic
export index
selftest
helpme
hw
status
shutdown
```

Expected UEFI behavior:

- local text console remains usable;
- storage repair/check reports the prepared `/vita/` tree state;
- journal and export files are written when FAT storage is available;
- audit readiness reports restricted diagnostic mode until UEFI persistent SQLite exists;
- audit verify/export/events honestly report unavailable/restricted status instead of claiming full SQLite audit;
- diagnostic and self-test reports are produced when storage is writable.

## 9. Framebuffer console validation

Inside UEFI, generate long output:

```text
helpme
hw
journal show
export index
diagnostic
selftest
```

Then test:

```text
PageUp
PageDown
mouse wheel up/down if firmware exposes wheel-compatible Z movement
clear
```

Expected:

- PageUp/PageDown scrollback works;
- wheel scroll works only when firmware exposes compatible pointer Z movement;
- if wheel is unavailable, keyboard scrollback remains usable;
- no GUI/window manager behavior is introduced.

## 10. Acceptance criteria

The current F1A/F1B base is considered valid when:

- `make clean && make hosted && make smoke-audit && make && make smoke && make iso` passes;
- hosted mode reports SQLite audit ready;
- UEFI mode does not claim full operational audit without persistent backend;
- `/vita/` storage tree can be checked/repaired;
- journal writes visible text/JSONL logs;
- audit verification and exports work in hosted;
- UEFI audit verification/export reports honest restricted status;
- diagnostic bundle and self-test reports are generated;
- export manifest indexes known report paths;
- no Python runtime/build dependency is introduced;
- no malloc is introduced in freestanding/UEFI patch paths;
- no GUI/window manager or real network/Wi-Fi expansion is introduced.

## 11. Known boundaries before the next phase

Still not included in F1A/F1B base closure:

- UEFI SQLite persistence;
- full VFS;
- full historical multi-boot audit export;
- audit row repair;
- compression/encryption of exports;
- remote upload;
- GUI/window manager;
- real Wi-Fi/network expansion.

## 12. Next phase entry condition

Only start the UEFI persistent audit backend phase after this checklist passes on the current tree.
