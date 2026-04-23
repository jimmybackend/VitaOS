#!/usr/bin/env bash
set -euo pipefail

DB_A=${1:-build/audit/vitaos-audit.db}
PEER_ENDPOINT=${2:-127.0.0.1:47001}
DB_B=${3:-build/audit/vitaos-peer.db}
LOG_DIR=build/test
LOG_A="$LOG_DIR/smoke-audit-a.log"
LOG_B="$LOG_DIR/vitanet-peer.log"
mkdir -p "$LOG_DIR"
rm -f "$DB_A" "$DB_B"

python3 tools/test/vitanet-peer.py "$PEER_ENDPOINT" "$DB_B" >"$LOG_B" 2>&1 &
PEER_PID=$!
trap 'kill "$PEER_PID" 2>/dev/null || true' EXIT
sleep 1

printf 'status\npeers\nproposals\napprove N-1\ntasks\nreplicate\ntasks pending\naudit\nshutdown\n' | ./build/hosted/vitaos-hosted "$DB_A" "$PEER_ENDPOINT" >"$LOG_A" 2>&1

wait "$PEER_PID" || true
trap - EXIT

[[ -f "$DB_A" ]] || { echo "smoke failed: node A db not created" >&2; exit 1; }
[[ -f "$DB_B" ]] || { echo "smoke failed: node B db not created" >&2; exit 1; }

TASK_A=$(sqlite3 "$DB_A" "select count(*) from node_task;")
TASK_B=$(sqlite3 "$DB_B" "select count(*) from node_task;")
PEER_A=$(sqlite3 "$DB_A" "select count(*) from node_peer;")
PEER_B=$(sqlite3 "$DB_B" "select count(*) from node_peer;")
IMPORTED_A=$(sqlite3 "$DB_A" "select count(*) from audit_event where event_type='AUDIT_REPL_IMPORTED';")
IMPORTED_B=$(sqlite3 "$DB_B" "select count(*) from audit_event where event_type='AUDIT_REPL_IMPORTED';")

[[ "$TASK_A" -ge 2 ]] || { echo "smoke failed: node A expected >=2 node_task" >&2; exit 1; }
[[ "$TASK_B" -ge 2 ]] || { echo "smoke failed: node B expected >=2 node_task" >&2; exit 1; }
[[ "$PEER_A" -ge 1 ]] || { echo "smoke failed: node A expected peer" >&2; exit 1; }
[[ "$PEER_B" -ge 1 ]] || { echo "smoke failed: node B expected peer" >&2; exit 1; }
[[ "$IMPORTED_A" -ge 1 ]] || { echo "smoke failed: node A expected imported audit events" >&2; exit 1; }
[[ "$IMPORTED_B" -ge 1 ]] || { echo "smoke failed: node B expected imported audit events" >&2; exit 1; }

for ev in NODE_TASK_CREATED NODE_TASK_SENT NODE_TASK_ACCEPTED NODE_TASK_COMPLETED AUDIT_REPLICATION_STARTED AUDIT_REPLICATION_RECEIVED AUDIT_REPLICATION_PERSISTED PEER_STATUS_REQUESTED PEER_STATUS_RECEIVED; do
  c=$(sqlite3 "$DB_A" "select count(*) from audit_event where event_type='${ev}';")
  [[ "$c" -ge 1 ]] || { echo "smoke failed: node A missing ${ev}" >&2; exit 1; }
done

for ev in NODE_TASK_ACCEPTED AUDIT_REPLICATION_RECEIVED; do
  c=$(sqlite3 "$DB_B" "select count(*) from audit_event where event_type='${ev}';")
  [[ "$c" -ge 1 ]] || { echo "smoke failed: node B missing ${ev}" >&2; exit 1; }
done

python - "$DB_A" <<'PY'
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
