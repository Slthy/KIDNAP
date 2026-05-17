#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# shellcheck source=afl_common.sh
source "$ROOT_DIR/scripts/afl_common.sh"

usage() {
    cat <<'MSG'
usage: run_feedback_comparison.sh [options]

Run a fixed-duration AFL++ comparison between the plain baseline target and the
syscall-feedback-aware target. The script builds separate target binaries,
runs one baseline campaign and one feedback-aware campaign with the same seed
corpus, then writes replay/queue summaries, a side-by-side summary CSV, and
feedback-growth CSV/PNG artifacts.

Options:
  -i DIR   seed input directory (default: ./in)
  -o DIR   comparison output directory (default: ./comparison-runs)
  -d SEC   seconds per AFL++ campaign, passed to afl-fuzz -V (default: 60)
  -t MS    AFL timeout in milliseconds (default: 1000)
  -h       show this help

Environment overrides:
  AFL_FUZZ=/path/to/afl-fuzz          choose the fuzzer binary
  AFL_CC=/path/to/afl-cc              choose the AFL compiler wrapper
  AFL_SKIP_CPUFREQ=1                  commonly useful on shared/big VMs
  AFL_I_DONT_CARE_ABOUT_MISSING_CRASHES=1  bypass pipe core_pattern checks
MSG
}

IN_DIR="$ROOT_DIR/in"
RUNS_DIR="$ROOT_DIR/comparison-runs"
DURATION_SEC=60
TIMEOUT_MS=1000

while getopts ':i:o:d:t:h' opt; do
    case "$opt" in
        i) IN_DIR="$OPTARG" ;;
        o) RUNS_DIR="$OPTARG" ;;
        d) DURATION_SEC="$OPTARG" ;;
        t) TIMEOUT_MS="$OPTARG" ;;
        h) usage; exit 0 ;;
        :) echo "error: -$OPTARG requires an argument" >&2; usage >&2; exit 2 ;;
        \?) echo "error: unknown option -$OPTARG" >&2; usage >&2; exit 2 ;;
    esac
done

if ! [[ "$DURATION_SEC" =~ ^[0-9]+$ && "$TIMEOUT_MS" =~ ^[0-9]+$ ]]; then
    echo 'error: -d and -t must be non-negative integers' >&2
    exit 2
fi

if [[ "$DURATION_SEC" -eq 0 ]]; then
    echo 'error: -d must be greater than zero so afl-fuzz can stop automatically' >&2
    exit 2
fi

AFL_FUZZ_BIN="$(resolve_afl_fuzz)"

mkdir -p "$IN_DIR" "$RUNS_DIR/bin" "$RUNS_DIR/reports" "$RUNS_DIR/logs"
if ! find "$IN_DIR" -maxdepth 1 -type f -print -quit | grep -q .; then
    printf '\x00\x00\x00\x00' > "$IN_DIR/seed"
fi

build_target_copy() {
    local make_target="$1"
    local out_bin="$2"

    if AFL_CC_BIN="$(resolve_afl_cc "$AFL_FUZZ_BIN")"; then
        run_make -C "$ROOT_DIR" clean
        if [[ -n "$make_target" ]]; then
            run_make -C "$ROOT_DIR" CC="$AFL_CC_BIN" "$make_target"
        else
            run_make -C "$ROOT_DIR" CC="$AFL_CC_BIN"
        fi
    else
        echo 'warning: AFL compiler wrapper not found; building with default compiler' >&2
        run_make -C "$ROOT_DIR" clean
        if [[ -n "$make_target" ]]; then
            run_make -C "$ROOT_DIR" "$make_target"
        else
            run_make -C "$ROOT_DIR"
        fi
    fi

    cp "$ROOT_DIR/poc_one" "$out_bin"
}

run_campaign() {
    local label="$1"
    local target_bin="$2"
    local out_dir="$3"
    local log_file="$4"

    mkdir -p "$out_dir" "$(dirname "$log_file")"
    echo "[*] Running $label campaign for ${DURATION_SEC}s -> $out_dir"
    prepare_afl_runtime_env
    env -u AFL_FUZZ "$AFL_FUZZ_BIN" -i "$IN_DIR" -o "$out_dir" -V "$DURATION_SEC" -t "$TIMEOUT_MS" -- "$target_bin" @@ \
        2>&1 | tee "$log_file"
}

BASELINE_BIN="$RUNS_DIR/bin/poc_one_baseline"
FEEDBACK_BIN="$RUNS_DIR/bin/poc_one_sysfeedback"
BASELINE_OUT="$RUNS_DIR/baseline"
FEEDBACK_OUT="$RUNS_DIR/sysfeedback"

export AFL_SKIP_CPUFREQ="${AFL_SKIP_CPUFREQ:-1}"
export AFL_I_DONT_CARE_ABOUT_MISSING_CRASHES="${AFL_I_DONT_CARE_ABOUT_MISSING_CRASHES:-1}"

rm -rf "$BASELINE_OUT" "$FEEDBACK_OUT"

echo "[*] Building baseline target copy: $BASELINE_BIN"
build_target_copy '' "$BASELINE_BIN"
echo "[*] Building syscall-feedback-aware target copy: $FEEDBACK_BIN"
build_target_copy sysfeedback "$FEEDBACK_BIN"

run_campaign baseline "$BASELINE_BIN" "$BASELINE_OUT" "$RUNS_DIR/logs/baseline.log"
run_campaign syscall-feedback "$FEEDBACK_BIN" "$FEEDBACK_OUT" "$RUNS_DIR/logs/sysfeedback.log"

BASELINE_REPORT="$RUNS_DIR/reports/baseline.json"
FEEDBACK_REPORT="$RUNS_DIR/reports/sysfeedback.json"
GROWTH_CSV="$RUNS_DIR/reports/feedback_growth.csv"
GROWTH_PNG="$RUNS_DIR/reports/feedback_growth.png"
SUMMARY_CSV="$RUNS_DIR/reports/summary.csv"

"$ROOT_DIR/scripts/analyze_queue.py" --format json --target "$BASELINE_BIN" "$BASELINE_OUT" > "$BASELINE_REPORT"
"$ROOT_DIR/scripts/analyze_queue.py" --format json --target "$FEEDBACK_BIN" "$FEEDBACK_OUT" > "$FEEDBACK_REPORT"
"$ROOT_DIR/scripts/plot_feedback.py" --no-plot --extend-to "$DURATION_SEC" --csv "$GROWTH_CSV" "$BASELINE_OUT" "$FEEDBACK_OUT" >/dev/null
if "$ROOT_DIR/scripts/plot_feedback.py" --extend-to "$DURATION_SEC" --csv "$GROWTH_CSV" --output "$GROWTH_PNG" "$BASELINE_OUT" "$FEEDBACK_OUT" >/dev/null; then
    PLOT_STATUS="[+] Growth PNG:                  $GROWTH_PNG"
else
    PLOT_STATUS="[!] Growth PNG skipped; install matplotlib to enable plotting"
fi

python3 - "$BASELINE_REPORT" "$FEEDBACK_REPORT" "$SUMMARY_CSV" <<'PY'
import csv
import json
import sys

metrics = [
    "files",
    "total_operations",
    "unique_syscalls",
    "unique_seq2",
    "unique_seq3",
    "unique_syscall_errno",
    "unique_prev_syscall_errno",
]
with open(sys.argv[1], "r", encoding="utf-8") as handle:
    baseline = json.load(handle)
with open(sys.argv[2], "r", encoding="utf-8") as handle:
    feedback = json.load(handle)
rows = []
for metric in metrics:
    base_value = baseline.get(metric, 0)
    feedback_value = feedback.get(metric, 0)
    rows.append({
        "metric": metric,
        "baseline": base_value,
        "sysfeedback": feedback_value,
        "delta": feedback_value - base_value,
        "ratio": "" if base_value == 0 else f"{feedback_value / base_value:.3f}",
    })
with open(sys.argv[3], "w", newline="", encoding="utf-8") as handle:
    writer = csv.DictWriter(handle, fieldnames=list(rows[0]))
    writer.writeheader()
    writer.writerows(rows)
PY

cat <<MSG
[+] Comparison complete.
[+] Baseline AFL output:         $BASELINE_OUT
[+] Feedback-aware AFL output:   $FEEDBACK_OUT
[+] Baseline queue report:       $BASELINE_REPORT
[+] Feedback-aware queue report: $FEEDBACK_REPORT
[+] Summary CSV:                 $SUMMARY_CSV
[+] Growth CSV:                  $GROWTH_CSV
$PLOT_STATUS
MSG
