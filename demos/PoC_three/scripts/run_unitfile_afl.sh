#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
IN_DIR="${1:-$ROOT_DIR/unit-seeds}"
OUT_DIR="${2:-$ROOT_DIR/out-unitfile-afl}"
AFL_FUZZ_BIN="${AFL_FUZZ:-afl-fuzz}"
export AFL_I_DONT_CARE_ABOUT_MISSING_CRASHES="${AFL_I_DONT_CARE_ABOUT_MISSING_CRASHES:-1}"
DURATION="${DURATION:-}"
DURATION_ARGS=()
if [[ -n "$DURATION" ]]; then
  DURATION_ARGS=(-V "$DURATION")
fi

exec "$AFL_FUZZ_BIN" "${DURATION_ARGS[@]}" -i "$IN_DIR" -o "$OUT_DIR" -- "$ROOT_DIR/unit_file_harness" @@
