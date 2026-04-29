#!/usr/bin/env bash
set -euo pipefail

make clean
make hosted
make smoke-audit
make

./tools/test/validate-storage-persistence.sh
./tools/test/validate-console-editor-history.sh

echo "validate-vitaos: ok"
