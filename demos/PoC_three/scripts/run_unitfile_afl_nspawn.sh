#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DURATION="${3:-${DURATION:-}}"
map_path() {
  local input="$1"
  if [[ "$input" = /* ]]; then
    case "$input" in
      "$ROOT_DIR"/*) printf '/opt/systemctl-ftrace-poc/%s\n' "${input#$ROOT_DIR/}" ;;
      *) echo "path must live under $ROOT_DIR so nspawn can see it: $input" >&2; exit 1 ;;
    esac
  else
    printf '%s\n' "$input"
  fi
}
IN_DIR="$(map_path "${1:-./unit-seeds}")"
OUT_ARG="${2:-./out-unitfile-afl}"
OUT_DIR="$(map_path "$OUT_ARG")"
if [[ -n "$DURATION" ]]; then
  "$ROOT_DIR/scripts/nspawn_exec.sh" env DURATION="$DURATION" ./scripts/run_unitfile_afl.sh "$IN_DIR" "$OUT_DIR"
else
  "$ROOT_DIR/scripts/nspawn_exec.sh" ./scripts/run_unitfile_afl.sh "$IN_DIR" "$OUT_DIR"
fi
if [[ "$OUT_ARG" = /* ]]; then
  host_out="$OUT_ARG"
else
  host_out="$ROOT_DIR/$OUT_ARG"
fi
sudo chown -R "$(id -u):$(id -g)" "$host_out"
