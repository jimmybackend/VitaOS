# VitaOS persistent session journal

This F1A/F1B journal is a visible bridge until full UEFI SQLite persistence exists.

It writes:

```text
/vita/audit/session-journal.jsonl
/vita/audit/session-journal.txt
```

It is not a hidden keylogger. It records submitted commands after Enter and summarized system responses/events. Sensitive-looking commands are redacted.

## Commands

```text
journal
journal status
journal show
journal flush
journal paths
journal note text
journal help
```

## Read from Linux

```bash
cat /media/$USER/VITAOS_EFI/vita/audit/session-journal.txt
cat /media/$USER/VITAOS_EFI/vita/audit/session-journal.jsonl
```

## Read from Windows

Open the USB drive and browse to:

```text
vita\audit\session-journal.txt
vita\audit\session-journal.jsonl
```

The TXT file opens in Notepad. The JSONL file is suitable for VS Code, Notepad++, PowerShell, or a future VitaOS viewer app.

## Limits

No SQLite, no hash chain, no encryption, no full raw console transcript yet. The journal uses bounded in-memory buffers and rewrites the two files after events.
