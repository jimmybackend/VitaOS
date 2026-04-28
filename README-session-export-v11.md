# Patch v11 — export: add session summary report

This patch adds a minimal text export command for VitaOS F1A/F1B.

## Added files

```text
include/vita/session_export.h
kernel/session_export.c
docs/storage/session-export.md
tools/patches/apply-session-export-v11.py
README-session-export-v11.md
```

## Modified by patch script

```text
Makefile
kernel/command_core.c
kernel/console.c
```

## Commands added

```text
export
export session
export report
session export
```

## Output file

```text
/vita/export/reports/last-session.txt
```

## What the report contains

- boot mode
- architecture
- audit state
- restricted diagnostic state
- proposal/node state
- hardware counters
- storage backend status
- network readiness summary
- AI gateway stage summary
- notes/messages/emergency paths
- current limitations

## Apply

```bash
cd ~/VitaOS
unzip -o ~/vitaos-session-export-v11.zip -d .
python3 tools/patches/apply-session-export-v11.py
```

## Build

```bash
make clean && \
make hosted && \
make smoke-audit && \
make && \
make iso
```

## Test hosted

```bash
./build/hosted/vitaos-hosted
```

Inside VitaOS:

```text
storage status
export session
cat /vita/export/reports/last-session.txt
shutdown
```

Host verification:

```bash
cat build/storage/vita/export/reports/last-session.txt
```

## Commit

```bash
git add include/vita/session_export.h \
        kernel/session_export.c \
        kernel/command_core.c \
        kernel/console.c \
        Makefile \
        docs/storage/session-export.md \
        tools/patches/apply-session-export-v11.py \
        README-session-export-v11.md

git commit -m "export: add session summary report"
```
