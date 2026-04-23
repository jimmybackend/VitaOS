#!/usr/bin/env python3
import socket
import sys
import time

bind = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1:47001"
host, port = bind.split(":", 1)
port = int(port)

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind((host, port))
sock.settimeout(4.0)

peer = None
start = time.time()
while time.time() - start < 4.0:
    try:
        data, addr = sock.recvfrom(4096)
    except socket.timeout:
        continue
    msg = data.decode(errors="replace").strip()
    peer = addr
    if msg.startswith("HELLO "):
        sock.sendto(b"HELLO peer-test-1", addr)
        sock.sendto(b"CAPS {\"audit_replication_v0\":true,\"first_aid\":true}", addr)
        sock.sendto(f"HEARTBEAT {int(time.time())}".encode(), addr)
    elif msg.startswith("AUDIT_BLOCK "):
        # semi-real replication baseline: block is received by peer harness.
        sock.sendto(f"HEARTBEAT repl-ack-{int(time.time())}".encode(), addr)
    elif msg.startswith("LINK_ACCEPT") or msg.startswith("LINK_REJECT"):
        sock.sendto(f"HEARTBEAT link-update-{int(time.time())}".encode(), addr)

sock.close()
