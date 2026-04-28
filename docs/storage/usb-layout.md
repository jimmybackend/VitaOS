# VitaOS USB writable layout

This document describes the writable directory tree prepared inside VitaOS USB images.

Patch: `usb: prepare VitaOS writable directory tree`
Scope: F1A/F1B storage preparation only.

This layout prepares a predictable FAT32 tree for future editor, audit persistence, network, and AI gateway work. It does not implement a kernel VFS, SQLite persistence in UEFI, a note editor, or real network transport yet.

## Top-level layout

```text
/EFI/
  /BOOT/
    BOOTX64.EFI

/img/
  1.bmp
  2.bmp
  3.bmp
  4.bmp

/vita/
  README.txt
  /config/
  /audit/
  /notes/
  /messages/
  /emergency/
  /ai/
  /net/
  /export/
  /tmp/
```

## `/EFI/BOOT/`

UEFI removable-media boot path.

Expected file:

```text
/EFI/BOOT/BOOTX64.EFI
```

This is the VitaOS UEFI application loaded by firmware.

## `/img/`

Boot splash frames used by the current UEFI splash path.

Expected files:

```text
/img/1.bmp
/img/2.bmp
/img/3.bmp
/img/4.bmp
```

If these files are missing, the UEFI splash path should fail silently and continue to text boot.

## `/vita/`

Root of writable VitaOS runtime data on the USB image.

This directory is intentionally separate from `/EFI` and `/img` so future runtime data does not mix with bootloader files.

Expected file:

```text
/vita/README.txt
```

The USB image builder copies this document there as a user-visible map of the data area.

## `/vita/config/`

Configuration placeholders for future runtime settings.

Initial files:

```text
/vita/config/vitaos.cfg
/vita/config/network.cfg
/vita/config/ai.cfg
```

Intended future use:

- language preference;
- console mode preference;
- default network mode;
- AI gateway endpoint;
- safe feature flags.

No secrets should be stored here until VitaOS has a clear credential and audit policy.

## `/vita/audit/`

Persistent audit target area.

Intended future files:

```text
/vita/audit/vitaos-audit.db
/vita/audit/boot-events.log
/vita/audit/pending-events.jsonl
```

Purpose:

- SQLite audit database when UEFI persistence is implemented;
- emergency fallback event logs;
- exportable audit blocks.

Current status:

- prepared only;
- UEFI SQLite persistence is not implemented yet.

## `/vita/notes/`

Human notes created from VitaOS.

Initial file:

```text
/vita/notes/README.txt
```

Intended future files:

```text
/vita/notes/note-0001.txt
/vita/notes/note-0002.txt
```

Future commands may include:

```text
note
notes
edit <path>
cat <path>
```

The first editor should be line-oriented and UEFI-safe, using commands like `.save`, `.cancel`, and `.help` instead of depending on complex key combinations.

## `/vita/messages/`

Message queues for future peer and AI gateway communication.

Subdirectories:

```text
/vita/messages/inbox/
/vita/messages/outbox/
/vita/messages/drafts/
```

Intended future use:

- messages received from nodes;
- messages waiting for network availability;
- drafts written by the operator;
- AI gateway request/response envelopes if needed.

## `/vita/emergency/`

Emergency-session data.

Subdirectories:

```text
/vita/emergency/reports/
/vita/emergency/checklists/
```

Initial file:

```text
/vita/emergency/README.txt
```

Intended future use:

- emergency reports created from the guided emergency flow;
- checklists for first aid, lost-person response, water, shelter, navigation;
- last session summaries.

## `/vita/ai/`

AI gateway and local assistant data.

Subdirectories:

```text
/vita/ai/prompts/
/vita/ai/responses/
/vita/ai/sessions/
```

Intended future use:

- prompts sent to a remote assistant gateway;
- responses received from a server;
- offline fallback session transcripts;
- proposal context prepared for human approval or rejection.

Current status:

- prepared only;
- remote AI network calls are not implemented yet.

## `/vita/net/`

Network status and profiles.

Subdirectories:

```text
/vita/net/profiles/
```

Intended future files:

```text
/vita/net/last-status.txt
/vita/net/profiles/ethernet-auto.cfg
```

Future use:

- Ethernet auto/manual configuration;
- last known network status;
- AI gateway network endpoint references.

Wi-Fi SSID/password storage should not be added until a clear credential handling policy exists.

## `/vita/export/`

Files prepared for external review on another system.

Subdirectories:

```text
/vita/export/audit/
/vita/export/notes/
/vita/export/reports/
```

Intended future use:

- audit database dumps;
- JSONL event exports;
- human notes;
- emergency reports.

## `/vita/tmp/`

Temporary runtime files.

Intended future use:

- scratch files;
- temporary exports;
- transient editor buffers.

Files here are not considered durable evidence or audit records.

## Implementation notes

The current image builder uses mtools to create this layout directly inside the FAT32 partition image. This avoids loop mounts and sudo during image creation.

The current layout is a preparation step. Real runtime writing from VitaOS still requires a future storage layer, likely using UEFI Simple File System first, then a VitaOS storage/VFS abstraction later.

## Future patch sequence

Recommended order:

```text
1. usb: prepare VitaOS writable directory tree
2. storage: add minimal UEFI FAT file writer
3. editor: add simple note editor command
4. console: add centered neon framebuffer terminal
5. audit: persist UEFI audit events to /vita/audit/
```
