#!/usr/bin/env bash
set -euo pipefail

DB_PATH=${1:-build/audit/vitaos-audit.db}
PEER_ENDPOINT=${2:-127.0.0.1:47001}
LOG_DIR=build/test
LOG_FILE="$LOG_DIR/smoke-audit.log"
PEER_LOG="$LOG_DIR/vitanet-peer.log"
mkdir -p "$LOG_DIR"
rm -f "$DB_PATH"

python3 tools/test/vitanet-peer.py "$PEER_ENDPOINT" >"$PEER_LOG" 2>&1 &
PEER_PID=$!
trap 'kill "$PEER_PID" 2>/dev/null || true' EXIT
sleep 1

printf 'status\npeers\nproposals\napprove N-1\nreject P-2\naudit\nshutdown\n' | ./build/hosted/vitaos-hosted "$DB_PATH" "$PEER_ENDPOINT" >"$LOG_FILE" 2>&1

wait "$PEER_PID" || true
trap - EXIT

if [[ ! -f "$DB_PATH" ]]; then
  echo "smoke failed: db not created" >&2
  exit 1
fi

BOOT_COUNT=$(sqlite3 "$DB_PATH" "select count(*) from boot_session;")
EVENT_COUNT=$(sqlite3 "$DB_PATH" "select count(*) from audit_event;")
HW_COUNT=$(sqlite3 "$DB_PATH" "select count(*) from hardware_snapshot;")
PROPOSAL_COUNT=$(sqlite3 "$DB_PATH" "select count(*) from ai_proposal;")
RESPONSE_COUNT=$(sqlite3 "$DB_PATH" "select count(*) from human_response;")
PEER_COUNT=$(sqlite3 "$DB_PATH" "select count(*) from node_peer;")
LINKED_COUNT=$(sqlite3 "$DB_PATH" "select count(*) from node_peer where link_state='linked';")

if [[ "$BOOT_COUNT" -lt 1 ]]; then echo "smoke failed: no boot_session" >&2; exit 1; fi
if [[ "$EVENT_COUNT" -lt 24 ]]; then echo "smoke failed: expected >=24 audit_event, got $EVENT_COUNT" >&2; exit 1; fi
if [[ "$HW_COUNT" -lt 1 ]]; then echo "smoke failed: no hardware_snapshot" >&2; exit 1; fi
if [[ "$PROPOSAL_COUNT" -lt 5 ]]; then echo "smoke failed: expected >=5 ai_proposal" >&2; exit 1; fi
if [[ "$RESPONSE_COUNT" -lt 2 ]]; then echo "smoke failed: expected >=2 human_response" >&2; exit 1; fi
if [[ "$PEER_COUNT" -lt 1 ]]; then echo "smoke failed: expected >=1 node_peer" >&2; exit 1; fi
if [[ "$LINKED_COUNT" -lt 1 ]]; then echo "smoke failed: expected linked node_peer after approve N-1" >&2; exit 1; fi

for event in BOOT_STARTED HANDOFF_TO_KMAIN CONSOLE_READY AUDIT_BACKEND_READY HW_DISCOVERY_STARTED HW_DISCOVERY_COMPLETED HW_SNAPSHOT_PERSISTED VITANET_STARTED PEER_DISCOVERED PEER_CAPABILITIES_RECEIVED LINK_PROPOSAL_CREATED LINK_ACCEPTED HEARTBEAT_RECEIVED AUDIT_REPLICATION_ATTEMPTED AI_PROPOSAL_CREATED AI_PROPOSAL_SHOWN AI_PROPOSAL_APPROVED AI_PROPOSAL_REJECTED GUIDED_CONSOLE_SHOWN MENU_OPTION_SELECTED; do
  COUNT=$(sqlite3 "$DB_PATH" "select count(*) from audit_event where event_type='${event}';")
  if [[ "$COUNT" -lt 1 ]]; then
    echo "smoke failed: missing event ${event}" >&2
    exit 1
  fi
done

python - "$DB_PATH" <<'PY'
import hashlib, sqlite3, sys
con = sqlite3.connect(sys.argv[1])
rows = con.execute("select boot_id,event_seq,event_type,severity,actor_type,summary,details_json,monotonic_ns,coalesce(prev_hash,''),event_hash from audit_event order by id").fetchall()
prev=""; seq=1
for r in rows:
    boot_id,s,etype,sev,actor,summary,details,mono,prev_hash,event_hash=r
    assert s==seq, f"event_seq discontinuity {s}!={seq}"
    assert prev_hash==prev, f"prev_hash discontinuity at seq {s}"
    payload=f"{boot_id}|{s}|{etype}|{sev}|{actor}|{summary}|{details}|{mono}|{prev_hash}"
    assert hashlib.sha256(payload.encode()).hexdigest()==event_hash, f"hash mismatch seq {s}"
    prev=event_hash; seq+=1
print('audit chain ok')
PY

echo "smoke-audit ok"
