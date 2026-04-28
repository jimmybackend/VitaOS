# VitaOS patch: net ethernet attempt v4

Patch 4 of the staged network/AI path.

## Goal

Add the first safe Ethernet/TCP connection attempt layer.

## Scope

Hosted:
- `net connect <ipv4> <port>` opens a TCP connection to an IPv4 literal.
- `net send <text>` sends one text line.
- `net disconnect` closes the socket.
- `net status` shows endpoint and connection state.

UEFI:
- Keeps diagnostic/status-only behavior.
- Accepts endpoint configuration intent.
- Does not claim real TCP/DHCP yet.

## Commands

```text
net
net status
net connect 192.168.1.10 8080
net send hello from VitaOS
net disconnect
```

## Not included

- DHCP
- DNS
- TLS/HTTPS
- UEFI TCP4 connect
- Wi-Fi SSID/password
- remote AI call
- GUI
- malloc
- new runtime dependencies

## Apply

```bash
cd ~/VitaOS
unzip -o ~/vitaos-net-ethernet-attempt-v4.zip -d .
python3 tools/patches/apply-net-ethernet-attempt-v4.py
```

## Test

```bash
make clean && \
make hosted && \
make smoke-audit && \
make && \
make iso
```

Hosted test with a local TCP listener:

```bash
nc -l 127.0.0.1 8080
```

In another terminal:

```bash
./build/hosted/vitaos-hosted
```

Then inside VitaOS:

```text
net connect 127.0.0.1 8080
net send hello from VitaOS
net status
net disconnect
shutdown
```

## Commit

```bash
git add include/vita/net_connect.h \
        kernel/net_connect.c \
        kernel/command_core.c \
        kernel/console.c \
        Makefile \
        tools/patches/apply-net-ethernet-attempt-v4.py \
        README-net-ethernet-attempt-v4.md

git commit -m "net: add first Ethernet connection attempt"
```
