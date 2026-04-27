# VitaOS patch v2 — detailed network readiness status

This patch adds a small `net_status` module and upgrades the existing `net` / `net status` command.

It is intentionally limited to F1A/F1B readiness reporting.

## Adds

- `include/vita/net_status.h`
- `kernel/net_status.c`
- `tools/patches/apply-net-status-details-v2.py`

## Modifies through patch script

- `Makefile`
- `kernel/command_core.c`
- `kernel/console.c`

## What the command reports

`net` now reports:

- hardware network counters from the existing hardware snapshot
- UEFI Simple Network Protocol presence
- UEFI IP4 service binding presence
- UEFI DHCP4 service binding presence
- UEFI TCP4 service binding presence
- UEFI UDP4 service binding presence
- whether a future Ethernet/DHCP attempt looks realistic on that boot path
- whether a future remote AI gateway is realistic after network bring-up exists

## What it does not do yet

- no DHCP
- no TCP connection
- no DNS
- no TLS
- no Wi-Fi SSID/password association
- no remote AI call
- no new drivers
- no malloc
- no GUI

## Apply

From repository root:

```bash
unzip -o ~/vitaos-net-status-details-v2.zip -d .
python3 tools/patches/apply-net-status-details-v2.py
```

## Test

```bash
make clean && \
make hosted && \
make smoke-audit && \
make && \
make iso
```

Inside VitaOS:

```text
net
net status
```

## Commit

```bash
git add include/vita/net_status.h \
        kernel/net_status.c \
        kernel/command_core.c \
        kernel/console.c \
        Makefile \
        tools/patches/apply-net-status-details-v2.py \
        README-net-status-details-v2.md

git commit -m "net: add UEFI network readiness details"
```
