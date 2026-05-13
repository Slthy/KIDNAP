#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# shellcheck source=afl_common.sh
source "$ROOT_DIR/scripts/afl_common.sh"
IN_DIR="${1:-$ROOT_DIR/in}"
OUT_DIR="${2:-$ROOT_DIR/out-baseline}"
TARGET="$ROOT_DIR/poc_one"

AFL_FUZZ_BIN="$(resolve_afl_fuzz)"

mkdir -p "$IN_DIR"
if ! find "$IN_DIR" -maxdepth 1 -type f -print -quit | grep -q .; then
    printf '\x00\x00\x00\x00' > "$IN_DIR/seed"
fi

if AFL_CC_BIN="$(resolve_afl_cc)"; then
    make -C "$ROOT_DIR" clean
    make -C "$ROOT_DIR" CC="$AFL_CC_BIN"
else
    echo "warning: AFL compiler wrapper not found; building with default compiler" >&2
    make -C "$ROOT_DIR" clean
    make -C "$ROOT_DIR"
fi

exec "$AFL_FUZZ_BIN" -i "$IN_DIR" -o "$OUT_DIR" -t 1000 -- "$TARGET" @@
