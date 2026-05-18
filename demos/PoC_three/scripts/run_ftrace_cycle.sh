#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
AFL_OUT="${1:-$ROOT_DIR/out-afl}"
REPORT_DIR="${2:-$ROOT_DIR/results/ftrace-cycle}"
TOP_K="${TOP_K:-20}"
PRESELECT_LIMIT="${PRESELECT_LIMIT:-64}"
MAX_REPLAY_OPS="${MAX_REPLAY_OPS:-12}"
MAX_EXPANDED_COMMANDS="${MAX_EXPANDED_COMMANDS:-24}"
REPLAY_PROFILE="${REPLAY_PROFILE:-fast-safe}"
REPLAY_TIMEOUT="${REPLAY_TIMEOUT:-15}"
HARNESS="${HARNESS:-$ROOT_DIR/scripts/nspawn_harness.sh}"
RESET_CMD="${RESET_CMD:-$ROOT_DIR/scripts/reset_state_nspawn.sh}"
PRESELECTED="$REPORT_DIR/preselected"

mkdir -p "$REPORT_DIR"
"$ROOT_DIR/scripts/preselect_queue.py" "$AFL_OUT" "$PRESELECTED" --limit "$PRESELECT_LIMIT" --max-ops "$MAX_REPLAY_OPS" --max-expanded-commands "$MAX_EXPANDED_COMMANDS" --profile "$REPLAY_PROFILE"
REPLAY_TIMEOUT="$REPLAY_TIMEOUT" sudo --preserve-env=REPLAY_TIMEOUT "$ROOT_DIR/scripts/analyze_queue.py" "$PRESELECTED" \
  --harness "$HARNESS" \
  --reset-cmd "$RESET_CMD" \
  --csv "$REPORT_DIR/queue-growth.csv" \
  > "$REPORT_DIR/queue-analysis.json"
"$ROOT_DIR/scripts/select_corpus.py" \
  "$REPORT_DIR/queue-growth.csv" "$PRESELECTED" "$REPORT_DIR/seeds-next" --top-k "$TOP_K"
