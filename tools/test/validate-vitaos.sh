#!/usr/bin/env bash
set -euo pipefail

make clean
make hosted
make smoke-audit
make

./tools/test/validate-storage-persistence.sh

echo "validate-vitaos: PASS"
