# VitaOS patch 3: AI gateway stub

This patch adds the first remote-assistant command layer for the F1A/F1B scope.

## Adds

- `include/vita/ai_gateway.h`
- `kernel/ai_gateway.c`
- `tools/patches/apply-ai-gateway-stub-v3.py`

The patch script modifies:

- `Makefile`
- `kernel/console.c`
- `kernel/command_core.c`

## Commands

```text
ai
ai status
ai help
ai connect <host> <port>
ai disconnect
ai ask <text>
```

## Scope

This is a command/status stub only.

It does not add:

- DHCP
- TCP connect
- DNS
- TLS/HTTPS
- Wi-Fi SSID/password association
- remote AI calls
- GUI
- malloc
- new dependencies

## Test

```bash
make clean && \
make hosted && \
make smoke-audit && \
make && \
make iso
```

Then try:

```text
ai status
ai connect 192.168.1.10 8080
ai ask tengo una emergencia y necesito ayuda
ai disconnect
```

## Suggested commit

```bash
git add include/vita/ai_gateway.h \
        kernel/ai_gateway.c \
        kernel/command_core.c \
        kernel/console.c \
        Makefile \
        tools/patches/apply-ai-gateway-stub-v3.py \
        README-ai-gateway-stub-v3.md

git commit -m "ai: add remote assistant gateway stub"
```
