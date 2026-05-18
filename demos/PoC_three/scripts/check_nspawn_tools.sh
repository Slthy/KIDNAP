#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
exec "$ROOT_DIR/scripts/nspawn_exec.sh" bash -lc '
  set -e
  command -v systemctl
  command -v gcc
  command -v make
  command -v afl-fuzz
  afl-fuzz -h >/dev/null 2>&1 || true
'
