#!/usr/bin/env python3
import json
import sqlite3
import socket
import sys
import time
from pathlib import Path

bind = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1:47001"
db_path = sys.argv[2] if len(sys.argv) > 2 else "build/audit/vitaos-peer.db"
host, port = bind.split(":", 1)
port = int(port)

Path(db_path).parent.mkdir(parents=True, exist_ok=True)
con = sqlite3.connect(db_path)
con.executescript(Path("schema/audit.sql").read_text())
boot_id = f"peer-boot-{int(time.time())}"
now = int(time.time())
con.execute(
    "insert or ignore into boot_session (boot_id,node_id,arch,boot_unix,boot_monotonic_ns,mode,audit_state,language_mode) values (?,?,?,?,?,?,?,?)",
    (boot_id, "peer-test-1", "x86_64", now, now * 1_000_000_000, "PEER", "READY", "es,en"),
)
con.commit()

def audit_event(event_type, summary):
    seq = con.execute("select coalesce(max(event_seq),0)+1 from audit_event where boot_id=?", (boot_id,)).fetchone()[0]
    now2 = int(time.time())
    con.execute(
        "insert into audit_event (boot_id,event_seq,event_type,severity,actor_type,summary,details_json,monotonic_ns,rtc_unix,prev_hash,event_hash) values (?,?,?,?,?,?,?,?,?,?,?)",
        (boot_id, seq, event_type, "INFO", "SYSTEM", summary, "{}", now2 * 1_000_000_000, now2, "", f"peerhash-{seq}"),
    )
    con.commit()

def upsert_peer(peer_id, link_state="discovered", caps="{}"):
    now2 = int(time.time())
    con.execute(
        "insert into node_peer (peer_id,first_seen_unix,last_seen_unix,transport,capabilities_json,trust_state,link_state) values (?,?,?,?,?,?,?) "
        "on conflict(peer_id) do update set last_seen_unix=excluded.last_seen_unix,capabilities_json=excluded.capabilities_json,link_state=excluded.link_state",
        (peer_id, now2, now2, "udp/ipv4-hosted", caps, "observed", link_state),
    )
    con.commit()

def upsert_task(task_id, origin, target, task_type, status, payload):
    now2 = int(time.time())
    con.execute(
        "insert into node_task (task_id,origin_node_id,target_node_id,task_type,status,created_unix,updated_unix,payload_json) values (?,?,?,?,?,?,?,?) "
        "on conflict(task_id) do update set status=excluded.status,updated_unix=excluded.updated_unix,payload_json=excluded.payload_json",
        (task_id, origin, target, task_type, status, now2, now2, payload),
    )
    con.commit()

def export_block():
    rows = con.execute("select event_seq,event_type,event_hash from audit_event where boot_id=? order by id desc limit 3", (boot_id,)).fetchall()
    return ";".join(f"{r[0]}:{r[1]}:{r[2]}" for r in rows)

def import_block(payload):
    for item in payload.split(";"):
        parts = item.split(":", 2)
        if len(parts) != 3:
            continue
        seq, etype, h = parts
        exists = con.execute("select count(*) from audit_event where summary=? and event_type='AUDIT_REPL_IMPORTED'", (h,)).fetchone()[0]
        if exists:
            continue
        now2 = int(time.time())
        nseq = con.execute("select coalesce(max(event_seq),0)+1 from audit_event where boot_id=?", (boot_id,)).fetchone()[0]
        con.execute(
            "insert into audit_event (boot_id,event_seq,event_type,severity,actor_type,summary,details_json,monotonic_ns,rtc_unix,prev_hash,event_hash) values (?,?,?,?,?,?,?,?,?,?,?)",
            (boot_id, nseq, "AUDIT_REPL_IMPORTED", "INFO", "SYSTEM", h, json.dumps({"remote_seq": seq, "remote_type": etype}), now2 * 1_000_000_000, now2, "", f"peerimp-{nseq}"),
        )
    con.commit()

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind((host, port))
sock.settimeout(8.0)
audit_event("PEER_HARNESS_STARTED", bind)

start = time.time()
while time.time() - start < 8.0:
    try:
        data, addr = sock.recvfrom(4096)
    except socket.timeout:
        continue
    msg = data.decode(errors="replace").strip()

    if msg.startswith("HELLO "):
        peer_id = msg.split(" ", 1)[1]
        upsert_peer(peer_id)
        audit_event("PEER_DISCOVERED", peer_id)
        sock.sendto(b"HELLO peer-test-1", addr)
        sock.sendto(b"CAPS {\"audit_replication_v0\":true,\"peer_status\":true}", addr)
        sock.sendto(f"HEARTBEAT {int(time.time())}".encode(), addr)
        continue

    if msg.startswith("CAPS "):
        caps = msg.split(" ", 1)[1]
        upsert_peer("node-local", caps=caps)
        audit_event("PEER_CAPABILITIES_RECEIVED", "node-local")
        continue

    if msg.startswith("LINK_ACCEPT"):
        upsert_peer("node-local", link_state="linked")
        audit_event("LINK_ACCEPTED", "node-local")
        continue

    if msg.startswith("LINK_REJECT"):
        upsert_peer("node-local", link_state="rejected")
        audit_event("LINK_REJECTED", "node-local")
        continue

    if msg.startswith("TASK "):
        body = msg[5:]
        parts = body.split("|", 2)
        if len(parts) == 3:
            task_type, task_id, payload = parts
            upsert_task(task_id, "node-local", "peer-test-1", task_type, "accepted", payload)
            audit_event("NODE_TASK_ACCEPTED", task_id)
            sock.sendto(f"TASK_ACK {task_id}".encode(), addr)
            if task_type == "peer_status_request":
                upsert_task(task_id, "node-local", "peer-test-1", task_type, "done", payload)
                audit_event("PEER_STATUS_REQUESTED", task_id)
                audit_event("PEER_STATUS_RECEIVED", task_id)
                sock.sendto(f"TASK_DONE {task_id}".encode(), addr)
            elif task_type == "audit_replicate_range":
                upsert_task(task_id, "node-local", "peer-test-1", task_type, "done", payload)
                audit_event("AUDIT_REPLICATION_STARTED", task_id)
                sock.sendto(f"AUDIT_BLOCK {export_block()}".encode(), addr)
                sock.sendto(f"TASK_DONE {task_id}".encode(), addr)
                audit_event("AUDIT_REPLICATION_PERSISTED", task_id)
            else:
                upsert_task(task_id, "node-local", "peer-test-1", task_type, "rejected", payload)
                audit_event("NODE_TASK_REJECTED", task_id)
                sock.sendto(f"TASK_REJECT {task_id}".encode(), addr)
        continue

    if msg.startswith("AUDIT_BLOCK "):
        import_block(msg[12:])
        audit_event("AUDIT_REPLICATION_RECEIVED", "node-local")
        sock.sendto(f"HEARTBEAT repl-ack-{int(time.time())}".encode(), addr)
        continue

sock.close()
con.close()
