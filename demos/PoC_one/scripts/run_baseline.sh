#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
IN_DIR="${1:-$ROOT_DIR/in}"
OUT_DIR="${2:-$ROOT_DIR/out-baseline}"
TARGET="$ROOT_DIR/poc_one"

if ! command -v afl-fuzz >/dev/null 2>&1; then
    echo "error: afl-fuzz is not on PATH" >&2
    exit 127
fi

mkdir -p "$IN_DIR"
if ! find "$IN_DIR" -maxdepth 1 -type f -print -quit | grep -q .; then
    printf '\x00\x00\x00\x00' > "$IN_DIR/seed"
fi

if command -v afl-cc >/dev/null 2>&1; then
    make -C "$ROOT_DIR" clean
    make -C "$ROOT_DIR" CC=afl-cc
else
    echo "warning: afl-cc not found; building with default compiler" >&2
    make -C "$ROOT_DIR" clean
    make -C "$ROOT_DIR"
fi

exec afl-fuzz -i "$IN_DIR" -o "$OUT_DIR" -t 1000 -- "$TARGET" @@
