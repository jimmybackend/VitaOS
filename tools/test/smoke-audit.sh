#!/usr/bin/env bash
set -euo pipefail

DB_PATH=${1:-build/audit/vitaos-audit.db}
LOG_DIR=build/test
LOG_FILE="$LOG_DIR/smoke-audit.log"
mkdir -p "$LOG_DIR"
rm -f "$DB_PATH"

./build/hosted/vitaos-hosted "$DB_PATH" >"$LOG_FILE" 2>&1 &
PID=$!
sleep 1
kill "$PID" >/dev/null 2>&1 || true
wait "$PID" >/dev/null 2>&1 || true

if [[ ! -f "$DB_PATH" ]]; then
  echo "smoke failed: db not created" >&2
  exit 1
fi

BOOT_COUNT=$(sqlite3 "$DB_PATH" "select count(*) from boot_session;")
EVENT_COUNT=$(sqlite3 "$DB_PATH" "select count(*) from audit_event;")

if [[ "$BOOT_COUNT" -lt 1 ]]; then
  echo "smoke failed: no boot_session" >&2
  exit 1
fi
if [[ "$EVENT_COUNT" -lt 4 ]]; then
  echo "smoke failed: expected >=4 audit_event, got $EVENT_COUNT" >&2
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
