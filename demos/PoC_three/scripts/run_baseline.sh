#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
RESULTS_DIR="${1:-$ROOT_DIR/results/baseline}"
mkdir -p "$RESULTS_DIR"

RESET_CMD="${RESET_CMD:-$ROOT_DIR/scripts/reset_state_nspawn.sh}"
HARNESS_RUNNER="${HARNESS_RUNNER:-$ROOT_DIR/scripts/nspawn_harness.sh}"

"$RESET_CMD"

for seed in baseline; do
  sudo "$ROOT_DIR/scripts/collect_ftrace.sh" "$RESULTS_DIR/$seed.trace" -- \
    "$HARNESS_RUNNER" "$ROOT_DIR/seeds/$seed"
  "$ROOT_DIR/scripts/analyze_trace.py" "$RESULTS_DIR/$seed.trace" \
    > "$RESULTS_DIR/$seed.json"
done
