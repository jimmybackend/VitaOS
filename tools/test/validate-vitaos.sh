#!/usr/bin/env bash
set -euo pipefail

run_with_timeout() {
  local seconds=$1
  shift
  if ! timeout "$seconds" "$@"; then
    rc=$?
    if [[ $rc -eq 124 ]]; then
      echo "validate-vitaos: timeout after ${seconds}s: $*" >&2
    fi
    return $rc
  fi
}

make clean
make hosted
make smoke-audit
make

run_with_timeout 300 ./tools/test/validate-storage-persistence.sh
run_with_timeout 120 ./tools/test/validate-console-editor-history.sh

echo "validate-vitaos: ok"
