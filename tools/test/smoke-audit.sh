#!/usr/bin/env bash
set -euo pipefail

DB_PATH=${1:-build/audit/vitaos-audit.db}
LOG_DIR=build/test
LOG_FILE="$LOG_DIR/smoke-audit.log"
mkdir -p "$LOG_DIR"
rm -f "$DB_PATH"

./build/hosted/vitaos-hosted "$DB_PATH" >"$LOG_FILE" 2>&1

if [[ ! -f "$DB_PATH" ]]; then
  echo "smoke failed: db not created" >&2
  exit 1
fi

BOOT_COUNT=$(sqlite3 "$DB_PATH" "select count(*) from boot_session;")
EVENT_COUNT=$(sqlite3 "$DB_PATH" "select count(*) from audit_event;")
HW_COUNT=$(sqlite3 "$DB_PATH" "select count(*) from hardware_snapshot;")

if [[ "$BOOT_COUNT" -lt 1 ]]; then
  echo "smoke failed: no boot_session" >&2
  exit 1
fi
if [[ "$EVENT_COUNT" -lt 7 ]]; then
  echo "smoke failed: expected >=7 audit_event, got $EVENT_COUNT" >&2
  exit 1
fi
if [[ "$HW_COUNT" -lt 1 ]]; then
  echo "smoke failed: no hardware_snapshot" >&2
  exit 1
fi

for event in BOOT_STARTED HANDOFF_TO_KMAIN CONSOLE_READY AUDIT_BACKEND_READY HW_DISCOVERY_STARTED HW_DISCOVERY_COMPLETED HW_SNAPSHOT_PERSISTED; do
  COUNT=$(sqlite3 "$DB_PATH" "select count(*) from audit_event where event_type='${event}';")
  if [[ "$COUNT" -lt 1 ]]; then
    echo "smoke failed: missing event ${event}" >&2
    exit 1
  fi
done

RAM_BYTES=$(sqlite3 "$DB_PATH" "select coalesce(ram_bytes,0) from hardware_snapshot order by id desc limit 1;")
if [[ "$RAM_BYTES" -le 0 ]]; then
  echo "smoke failed: ram_bytes not detected" >&2
  exit 1
fi

python - "$DB_PATH" <<'PY'
import hashlib
import sqlite3
import sys

db = sys.argv[1]
con = sqlite3.connect(db)
rows = con.execute(
    """
    select boot_id,event_seq,event_type,severity,actor_type,summary,details_json,monotonic_ns,coalesce(prev_hash,''),event_hash
    from audit_event
    order by id asc
    """
).fetchall()

if not rows:
    raise SystemExit("no rows")

prev = ""
expected_seq = 1
for r in rows:
    boot_id, seq, etype, sev, actor, summary, details, mono, prev_hash, event_hash = r
    if seq != expected_seq:
        raise SystemExit(f"event_seq discontinuity: got {seq}, expected {expected_seq}")
    if prev_hash != prev:
        raise SystemExit(f"prev_hash discontinuity at seq {seq}")
    payload = f"{boot_id}|{seq}|{etype}|{sev}|{actor}|{summary}|{details}|{mono}|{prev_hash}"
    digest = hashlib.sha256(payload.encode()).hexdigest()
    if digest != event_hash:
        raise SystemExit(f"event_hash mismatch at seq {seq}")
    prev = event_hash
    expected_seq += 1

print("audit chain ok")
PY

echo "smoke-audit ok"
