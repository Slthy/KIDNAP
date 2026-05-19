#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ROUNDS="${ROUNDS:-3}"
DURATION="${DURATION:-300}"
TOP_K="${TOP_K:-20}"
PRESELECT_LIMIT="${PRESELECT_LIMIT:-64}"
MAX_REPLAY_OPS="${MAX_REPLAY_OPS:-12}"
MAX_EXPANDED_COMMANDS="${MAX_EXPANDED_COMMANDS:-24}"
REPLAY_PROFILE="${REPLAY_PROFILE:-fast-safe}"
REPLAY_TIMEOUT="${REPLAY_TIMEOUT:-15}"
TRACE_PROFILE="${TRACE_PROFILE:-syscalls}"
SELECT_MIN_SCORE="${SELECT_MIN_SCORE:-0}"
SELECT_STRATEGY="${SELECT_STRATEGY:-diverse}"
AFL_FUZZ_BIN="${AFL_FUZZ:-afl-fuzz}"
FUZZ_RUNNER="${FUZZ_RUNNER:-$ROOT_DIR/scripts/run_afl_nspawn.sh}"
HARNESS="${HARNESS:-$ROOT_DIR/scripts/nspawn_harness.sh}"
RESET_CMD="${RESET_CMD:-$ROOT_DIR/scripts/reset_state_nspawn.sh}"
SEEDS="${INITIAL_SEEDS:-$ROOT_DIR/seeds}"
BASE_OUT="${1:-$ROOT_DIR/results/reseed-loop}"

mkdir -p "$BASE_OUT"
for round in $(seq 1 "$ROUNDS"); do
  OUT="$BASE_OUT/round-$round"
  mkdir -p "$OUT"
  AFL_FUZZ="$AFL_FUZZ_BIN" "$FUZZ_RUNNER" "$SEEDS" "$OUT/afl" "$DURATION"
  PRESELECTED="$OUT/preselected"
  "$ROOT_DIR/scripts/preselect_queue.py" "$OUT/afl" "$PRESELECTED" --limit "$PRESELECT_LIMIT" --max-ops "$MAX_REPLAY_OPS" --max-expanded-commands "$MAX_EXPANDED_COMMANDS" --profile "$REPLAY_PROFILE"
  REPLAY_TIMEOUT="$REPLAY_TIMEOUT" TRACE_PROFILE="$TRACE_PROFILE" sudo --preserve-env=REPLAY_TIMEOUT,TRACE_PROFILE "$ROOT_DIR/scripts/analyze_queue.py" "$PRESELECTED" --harness "$HARNESS" --reset-cmd "$RESET_CMD" --trace-mode "$TRACE_PROFILE" --csv "$OUT/queue-growth.csv" > "$OUT/queue-analysis.json"
  NEXT="$OUT/seeds-next"
  "$ROOT_DIR/scripts/select_corpus.py" "$OUT/queue-growth.csv" "$PRESELECTED" "$NEXT" --top-k "$TOP_K" --min-score "$SELECT_MIN_SCORE" --strategy "$SELECT_STRATEGY"
  SEEDS="$NEXT"
done
