#!/usr/bin/env bash
set -euo pipefail

DB_PATH=${1:-build/audit/vitaos-audit.db}
TEST_DIR=build/test
LOG_FILE="$TEST_DIR/smoke-audit.log"
MOCK_LOG="$TEST_DIR/vitanet-mock.log"
REPL_FILE="$TEST_DIR/vitanet-audit-block.txt"

mkdir -p "$TEST_DIR" "$(dirname "$DB_PATH")"
rm -f "$DB_PATH" "$LOG_FILE" "$MOCK_LOG" "$REPL_FILE"

cleanup() {
  if [[ -n "${MOCK_PID:-}" ]]; then
    kill "$MOCK_PID" >/dev/null 2>&1 || true
    wait "$MOCK_PID" >/dev/null 2>&1 || true
  fi
}
trap cleanup EXIT

python3 - "$REPL_FILE" >"$MOCK_LOG" 2>&1 <<'PY' &
import socket
import sys
import time

outfile = sys.argv[1]
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind(("127.0.0.1", 47001))
sock.settimeout(0.25)

deadline = time.time() + 6.0
hello_sent = False
caps_sent = False
heartbeat_sent = False

while time.time() < deadline:
    try:
        data, addr = sock.recvfrom(2048)
    except socket.timeout:
        continue

    msg = data.decode("utf-8", errors="replace").strip()
    print(msg, flush=True)

    if msg.startswith("HELLO ") and not hello_sent:
        sock.sendto(b"HELLO peer-smoke", addr)
        hello_sent = True

    if msg.startswith("CAPS ") and not caps_sent:
        sock.sendto(b'CAPS {"audit_replication_v0":true,"node_task_v0":true,"peer_role":"smoke"}', addr)
        caps_sent = True

    if hello_sent and caps_sent and not heartbeat_sent:
        sock.sendto(b"HEARTBEAT", addr)
        heartbeat_sent = True

    if msg.startswith("AUDIT_BLOCK "):
        with open(outfile, "w", encoding="utf-8") as f:
            f.write(msg[len("AUDIT_BLOCK "):])
        break

sock.close()
PY
MOCK_PID=$!

sleep 0.2

printf "status\naudit\nproposals\npeers\nshutdown\n" | ./build/hosted/vitaos-hosted "$DB_PATH" >"$LOG_FILE" 2>&1

sleep 0.2
cleanup
trap - EXIT

if [[ ! -f "$DB_PATH" ]]; then
  echo "smoke failed: db not created" >&2
  exit 1
fi

if [[ ! -s "$REPL_FILE" ]]; then
  echo "smoke failed: audit replication block not captured" >&2
  echo "--- mock log ---" >&2
  cat "$MOCK_LOG" >&2 || true
  echo "--- hosted log ---" >&2
  cat "$LOG_FILE" >&2 || true
  exit 1
fi

BOOT_COUNT=$(sqlite3 "$DB_PATH" "select count(*) from boot_session;")
EVENT_COUNT=$(sqlite3 "$DB_PATH" "select count(*) from audit_event;")
HW_COUNT=$(sqlite3 "$DB_PATH" "select count(*) from hardware_snapshot;")
PROPOSAL_COUNT=$(sqlite3 "$DB_PATH" "select count(*) from ai_proposal;")
PEER_COUNT=$(sqlite3 "$DB_PATH" "select count(*) from node_peer;")
NODE_TASK_TABLE=$(sqlite3 "$DB_PATH" "select count(*) from sqlite_master where type='table' and name='node_task';")
HUMAN_RESPONSE_TABLE=$(sqlite3 "$DB_PATH" "select count(*) from sqlite_master where type='table' and name='human_response';")

if [[ "$BOOT_COUNT" -lt 1 ]]; then
  echo "smoke failed: no boot_session" >&2
  exit 1
fi

if [[ "$EVENT_COUNT" -lt 12 ]]; then
  echo "smoke failed: expected >=12 audit_event, got $EVENT_COUNT" >&2
  exit 1
fi

if [[ "$HW_COUNT" -lt 1 ]]; then
  echo "smoke failed: no hardware_snapshot" >&2
  exit 1
fi

if [[ "$PROPOSAL_COUNT" -lt 4 ]]; then
  echo "smoke failed: expected >=4 ai_proposal, got $PROPOSAL_COUNT" >&2
  exit 1
fi

if [[ "$PEER_COUNT" -lt 1 ]]; then
  echo "smoke failed: expected >=1 node_peer, got $PEER_COUNT" >&2
  exit 1
fi

if [[ "$NODE_TASK_TABLE" -lt 1 ]]; then
  echo "smoke failed: node_task table missing" >&2
  exit 1
fi

if [[ "$HUMAN_RESPONSE_TABLE" -lt 1 ]]; then
  echo "smoke failed: human_response table missing" >&2
  exit 1
fi

RAM_BYTES=$(sqlite3 "$DB_PATH" "select coalesce(ram_bytes,0) from hardware_snapshot order by id desc limit 1;")
if [[ "$RAM_BYTES" -le 0 ]]; then
  echo "smoke failed: ram_bytes not detected" >&2
  exit 1
fi

DISPLAY_COUNT=$(sqlite3 "$DB_PATH" "select coalesce(display_count,0) from hardware_snapshot order by id desc limit 1;")
if [[ "$DISPLAY_COUNT" -lt 1 ]]; then
  echo "smoke failed: display_count not detected" >&2
  exit 1
fi

AI_READY=$(sqlite3 "$DB_PATH" "select count(*) from audit_event where event_type='AI_PROPOSALS_READY';")
PEER_DISCOVERED=$(sqlite3 "$DB_PATH" "select count(*) from audit_event where event_type='PEER_DISCOVERED';")
AUDIT_REPL=$(sqlite3 "$DB_PATH" "select count(*) from audit_event where event_type='AUDIT_REPLICATION_ATTEMPTED';")
REPL_STARTED=$(sqlite3 "$DB_PATH" "select count(*) from audit_event where event_type='COMMAND_REPL_STARTED';")
REPL_STOPPED=$(sqlite3 "$DB_PATH" "select count(*) from audit_event where event_type='COMMAND_REPL_STOPPED';")

if [[ "$AI_READY" -lt 1 ]]; then
  echo "smoke failed: AI_PROPOSALS_READY not found" >&2
  exit 1
fi

if [[ "$PEER_DISCOVERED" -lt 1 ]]; then
  echo "smoke failed: PEER_DISCOVERED not found" >&2
  exit 1
fi

if [[ "$AUDIT_REPL" -lt 1 ]]; then
  echo "smoke failed: AUDIT_REPLICATION_ATTEMPTED not found" >&2
  exit 1
fi

if [[ "$REPL_STARTED" -lt 1 || "$REPL_STOPPED" -lt 1 ]]; then
  echo "smoke failed: command REPL events not found" >&2
  exit 1
fi

if ! grep -q "VitaOS with AI / VitaOS con IA" "$LOG_FILE"; then
  echo "smoke failed: banner not found in hosted log" >&2
  exit 1
fi

if ! grep -q "Interactive console ready / Consola interactiva lista" "$LOG_FILE"; then
  echo "smoke failed: interactive console banner not found in hosted log" >&2
  exit 1
fi

if ! grep -q "Guided status / Estado guiado:" "$LOG_FILE"; then
  echo "smoke failed: guided status not found in hosted log" >&2
  exit 1
fi

if ! grep -q "display_count:" "$LOG_FILE"; then
  echo "smoke failed: display_count not found in hosted log" >&2
  exit 1
fi

python3 - "$DB_PATH" <<'PY'
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
