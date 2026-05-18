#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
exec "$ROOT_DIR/scripts/nspawn_exec.sh" bash -lc '
  set -e
  if command -v afl-clang-fast >/dev/null 2>&1; then
    cc=afl-clang-fast
  elif command -v afl-cc >/dev/null 2>&1; then
    cc=afl-cc
  else
    echo "no AFL compiler wrapper found in nspawn" >&2
    exit 1
  fi
  make clean
  make CC="$cc"
'
