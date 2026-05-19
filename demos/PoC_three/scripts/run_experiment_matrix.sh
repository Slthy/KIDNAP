#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="${1:-$ROOT_DIR/results/experiment-matrix}"
DURATION="${DURATION:-300}"
ROUNDS="${ROUNDS:-3}"
TOP_K="${TOP_K:-20}"

BASELINE_DIR="$OUT/manual-baseline"
COMMAND_AFL_DIR="$OUT/command-afl"
COMMAND_RESEED_DIR="$OUT/command-reseed"
UNIT_AFL_DIR="$OUT/unitfile-afl"
UNIT_RESEED_DIR="$OUT/unitfile-reseed"
REPORT_DIR="$OUT/reports"
mkdir -p "$BASELINE_DIR" "$COMMAND_AFL_DIR" "$UNIT_AFL_DIR" "$REPORT_DIR"

# 1. Manual baseline
"$ROOT_DIR/scripts/run_baseline.sh" "$BASELINE_DIR"

# 2. Plain command-sequence AFL campaign + host-side replay
DURATION="$DURATION" "$ROOT_DIR/scripts/run_afl_nspawn.sh" ./seeds "$COMMAND_AFL_DIR/afl"
"$ROOT_DIR/scripts/run_ftrace_cycle.sh" "$COMMAND_AFL_DIR/afl" "$COMMAND_AFL_DIR/ftrace"

# 3. Reseeded command-sequence campaign
ROUNDS="$ROUNDS" DURATION="$DURATION" TOP_K="$TOP_K" \
  "$ROOT_DIR/scripts/run_reseed_loop.sh" "$COMMAND_RESEED_DIR"

# 4. Plain unit-file AFL campaign + host-side replay
DURATION="$DURATION" "$ROOT_DIR/scripts/run_unitfile_afl_nspawn.sh" ./unit-seeds "$UNIT_AFL_DIR/afl"
NSPAWN_TARGET_HARNESS=unit_file_harness \
  "$ROOT_DIR/scripts/run_ftrace_cycle.sh" "$UNIT_AFL_DIR/afl" "$UNIT_AFL_DIR/ftrace"

# 5. Reseeded unit-file campaign
NSPAWN_TARGET_HARNESS=unit_file_harness \
FUZZ_RUNNER="$ROOT_DIR/scripts/run_unitfile_afl_nspawn.sh" \
INITIAL_SEEDS="$ROOT_DIR/unit-seeds" \
ROUNDS="$ROUNDS" DURATION="$DURATION" TOP_K="$TOP_K" \
  "$ROOT_DIR/scripts/run_reseed_loop.sh" "$UNIT_RESEED_DIR"

command_reseed_csv="$COMMAND_RESEED_DIR/round-$ROUNDS/queue-growth.csv"
unit_reseed_csv="$UNIT_RESEED_DIR/round-$ROUNDS/queue-growth.csv"
command_round_args=()
unit_round_args=()
for round in $(seq 1 "$ROUNDS"); do
  command_round_args+=(--command-reseed-round "$COMMAND_RESEED_DIR/round-$round/queue-growth.csv")
  unit_round_args+=(--unit-reseed-round "$UNIT_RESEED_DIR/round-$round/queue-growth.csv")
done

"$ROOT_DIR/scripts/summarize_experiments.py" \
  --baseline-json "$BASELINE_DIR/baseline.json" \
  --command-afl "$COMMAND_AFL_DIR/ftrace/queue-growth.csv" \
  --command-reseed "$command_reseed_csv" \
  "${command_round_args[@]}" \
  --unit-afl "$UNIT_AFL_DIR/ftrace/queue-growth.csv" \
  --unit-reseed "$unit_reseed_csv" \
  "${unit_round_args[@]}" \
  --csv "$REPORT_DIR/summary.csv" \
  --json "$REPORT_DIR/summary.json" \
  > "$REPORT_DIR/summary.pretty.json"

if "$ROOT_DIR/scripts/plot_comparison.py" \
  --command-afl "$COMMAND_AFL_DIR/ftrace/queue-growth.csv" \
  --command-reseed "$command_reseed_csv" \
  "${command_round_args[@]}" \
  --unit-afl "$UNIT_AFL_DIR/ftrace/queue-growth.csv" \
  --unit-reseed "$unit_reseed_csv" \
  --output "$REPORT_DIR/event-growth-comparison.png"; then
  plot_message="Growth plot: $REPORT_DIR/event-growth-comparison.png"
else
  plot_message="Growth plot skipped (install matplotlib to enable it)."
fi

echo "Experiment matrix complete: $OUT"
echo "Summary CSV: $REPORT_DIR/summary.csv"
echo "$plot_message"
