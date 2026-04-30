#!/usr/bin/env bash
set -euo pipefail

DB_PATH=${1:-build/audit/vitaos-audit.db}
TEST_DIR=build/test
LOG_FILE="$TEST_DIR/smoke-audit.log"
MOCK_LOG="$TEST_DIR/vitanet-mock.log"
REPL_FILE="$TEST_DIR/vitanet-audit-block.txt"
MOCK_SRC="$TEST_DIR/vitanet-mock.c"
MOCK_BIN="$TEST_DIR/vitanet-mock"

mkdir -p "$TEST_DIR" "$(dirname "$DB_PATH")"
rm -f "$DB_PATH" "$LOG_FILE" "$MOCK_LOG" "$REPL_FILE" "$MOCK_SRC" "$MOCK_BIN"

cleanup() {
  if [[ -n "${MOCK_PID:-}" ]]; then
    kill "$MOCK_PID" >/dev/null 2>&1 || true
    wait "$MOCK_PID" >/dev/null 2>&1 || true
  fi
}
trap cleanup EXIT

cat >"$MOCK_SRC" <<'C_EOF'
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

int main(int argc, char **argv) {
    int fd;
    struct sockaddr_in addr;
    struct sockaddr_in from;
    socklen_t from_len = sizeof(from);
    char buf[2048];
    bool hello_sent = false;
    bool caps_sent = false;
    bool heartbeat_sent = false;
    const char *outfile;

    if (argc < 2) {
        fprintf(stderr, "usage: %s out_file\\n", argv[0]);
        return 2;
    }

    outfile = argv[1];

    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        perror("socket");
        return 1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(47001);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(fd);
        return 1;
    }

    for (;;) {
        ssize_t n = recvfrom(fd, buf, sizeof(buf) - 1, 0, (struct sockaddr *)&from, &from_len);
        if (n <= 0) {
            if (errno == EINTR) {
                continue;
            }
            break;
        }

        buf[n] = '\0';
        fprintf(stdout, "%s\\n", buf);
        fflush(stdout);

        if (strncmp(buf, "HELLO ", 6) == 0 && !hello_sent) {
            const char *reply = "HELLO peer-smoke";
            sendto(fd, reply, strlen(reply), 0, (struct sockaddr *)&from, from_len);
            hello_sent = true;
        }

        if (strncmp(buf, "CAPS ", 5) == 0 && !caps_sent) {
            const char *reply = "CAPS {\"audit_replication_v0\":true,\"node_task_v0\":true,\"peer_role\":\"smoke\"}";
            sendto(fd, reply, strlen(reply), 0, (struct sockaddr *)&from, from_len);
            caps_sent = true;
        }

        if (hello_sent && caps_sent && !heartbeat_sent) {
            const char *reply = "HEARTBEAT";
            sendto(fd, reply, strlen(reply), 0, (struct sockaddr *)&from, from_len);
            heartbeat_sent = true;
        }

        if (strncmp(buf, "AUDIT_BLOCK ", 12) == 0) {
            FILE *f = fopen(outfile, "w");
            if (f) {
                fputs(buf + 12, f);
                fclose(f);
            }
            break;
        }
    }

    close(fd);
    return 0;
}
C_EOF

${CC:-cc} -O2 -Wall -Wextra -Werror "$MOCK_SRC" -o "$MOCK_BIN"
"$MOCK_BIN" "$REPL_FILE" >"$MOCK_LOG" 2>&1 &
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

for marker in \
  "VitaIR-Tri runtime claims:" \
  "storage.persistent.writable" \
  "audit.journal_jsonl.available" \
  "audit.sqlite.available" \
  "operational.full"; do
  if ! grep -q "$marker" "$LOG_FILE"; then
    echo "smoke failed: VitaIR marker not found in hosted log: $marker" >&2
    exit 1
  fi
done

prev=""
expected_seq=1
while IFS=$'\t' read -r boot_id seq etype sev actor summary details mono prev_hash event_hash; do
  [[ -z "${seq:-}" ]] && continue

  if [[ "$seq" -ne "$expected_seq" ]]; then
    echo "smoke failed: event_seq discontinuity: got $seq, expected $expected_seq" >&2
    exit 1
  fi

  if [[ "$prev_hash" == "-" ]]; then
    prev_hash=""
  fi

  if [[ "$prev_hash" != "$prev" ]]; then
    echo "smoke failed: prev_hash discontinuity at seq $seq" >&2
    exit 1
  fi

  payload="${boot_id}|${seq}|${etype}|${sev}|${actor}|${summary}|${details}|${mono}|${prev_hash}"
  digest=$(printf '%s' "$payload" | sha256sum | awk '{print $1}')

  if [[ "$digest" != "$event_hash" ]]; then
    echo "smoke failed: event_hash mismatch at seq $seq" >&2
    exit 1
  fi

  prev="$event_hash"
  expected_seq=$((expected_seq + 1))
done < <(sqlite3 -tabs "$DB_PATH" "select boot_id,event_seq,event_type,severity,actor_type,summary,details_json,monotonic_ns,coalesce(nullif(prev_hash,''),'-'),event_hash from audit_event order by id asc;")

echo "smoke-audit ok"
