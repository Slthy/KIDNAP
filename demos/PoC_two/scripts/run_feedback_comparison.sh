#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

usage() {
    cat <<'MSG'
usage: run_feedback_comparison.sh [options]

Run a fixed-duration AFL++ comparison between the normal configlet target and
the CONFIGLET_FEEDBACK target. The script builds separate binaries, runs one
baseline campaign and one feedback-driven campaign with the same seeds and
dictionary, then writes queue analysis reports for both.

Options:
  -i DIR   seed input directory (default: ./seeds)
  -o DIR   comparison output directory (default: ./comparison-runs)
  -d SEC   seconds per AFL++ campaign, passed to afl-fuzz -V (default: 60)
  -t MS    AFL timeout in milliseconds (default: 1000)
  -h       show this help

Environment overrides:
  AFL_FUZZ=/path/to/afl-fuzz          choose the fuzzer binary
  AFL_CC=/path/to/afl-clang-fast      choose the AFL compiler wrapper
  AFL_SKIP_CPUFREQ=1                  commonly useful on shared/big VMs
  AFL_I_DONT_CARE_ABOUT_MISSING_CRASHES=1  bypass pipe core_pattern checks
MSG
}

resolve_afl_fuzz() {
    if [[ -n "${AFL_FUZZ:-}" ]]; then
        printf '%s\n' "$AFL_FUZZ"
        return
    fi
    command -v afl-fuzz
}

resolve_afl_cc() {
    if [[ -n "${AFL_CC:-}" ]]; then
        printf '%s\n' "$AFL_CC"
        return
    fi
    command -v afl-clang-fast || command -v afl-cc || command -v afl-gcc
}

IN_DIR="$ROOT_DIR/seeds"
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
AFL_CC_BIN="$(resolve_afl_cc)"

mkdir -p "$RUNS_DIR/bin" "$RUNS_DIR/reports" "$RUNS_DIR/logs"

build_target_copy() {
    local make_target="$1"
    local out_bin="$2"

    make -C "$ROOT_DIR" clean
    if [[ -n "$make_target" ]]; then
        make -C "$ROOT_DIR" CC="$AFL_CC_BIN" "$make_target"
    else
        make -C "$ROOT_DIR" CC="$AFL_CC_BIN"
    fi
    cp "$ROOT_DIR/configlet_sim" "$out_bin"
}

run_campaign() {
    local label="$1"
    local target_bin="$2"
    local out_dir="$3"
    local feedback_log="$4"
    local afl_log="$5"

    rm -rf "$out_dir" "$feedback_log"
    mkdir -p "$out_dir" "$(dirname "$feedback_log")" "$(dirname "$afl_log")"
    echo "[*] Running $label campaign for ${DURATION_SEC}s -> $out_dir"
    env \
        AFL_SKIP_CPUFREQ="${AFL_SKIP_CPUFREQ:-1}" \
        AFL_I_DONT_CARE_ABOUT_MISSING_CRASHES="${AFL_I_DONT_CARE_ABOUT_MISSING_CRASHES:-1}" \
        CONFIGLET_FEEDBACK_FILE="$feedback_log" \
        "$AFL_FUZZ_BIN" -i "$IN_DIR" -o "$out_dir" -V "$DURATION_SEC" -t "$TIMEOUT_MS" -x "$ROOT_DIR/dict/configlet.dict" -- "$target_bin" @@ \
        2>&1 | tee "$afl_log"
}

BASELINE_BIN="$RUNS_DIR/bin/configlet_baseline"
FEEDBACK_BIN="$RUNS_DIR/bin/configlet_feedback"
BASELINE_OUT="$RUNS_DIR/baseline"
FEEDBACK_OUT="$RUNS_DIR/configlet-feedback"

build_target_copy '' "$BASELINE_BIN"
build_target_copy feedback "$FEEDBACK_BIN"

run_campaign baseline "$BASELINE_BIN" "$BASELINE_OUT" "$RUNS_DIR/logs/feedback-baseline.log" "$RUNS_DIR/logs/baseline.afl.log"
run_campaign configlet-feedback "$FEEDBACK_BIN" "$FEEDBACK_OUT" "$RUNS_DIR/logs/feedback-configlet.log" "$RUNS_DIR/logs/configlet-feedback.afl.log"

BASELINE_REPORT="$RUNS_DIR/reports/baseline.json"
FEEDBACK_REPORT="$RUNS_DIR/reports/configlet-feedback.json"

"$ROOT_DIR/scripts/analyze_queue.py" --format json --target "$BASELINE_BIN" "$BASELINE_OUT" > "$BASELINE_REPORT"
"$ROOT_DIR/scripts/analyze_queue.py" --format json --target "$FEEDBACK_BIN" "$FEEDBACK_OUT" > "$FEEDBACK_REPORT"

python3 - "$BASELINE_REPORT" "$FEEDBACK_REPORT" "$RUNS_DIR/reports/summary.csv" <<'PY'
import csv
import json
import sys

rows = []
for label, path in (("baseline", sys.argv[1]), ("configlet-feedback", sys.argv[2])):
    with open(path, "r", encoding="utf-8") as handle:
        data = json.load(handle)
    rows.append({
        "label": label,
        "files": data["files"],
        "features": data["covered"]["features"],
        "combos": data["covered"]["combos"],
        "dependencies": data["covered"]["dependencies"],
        "total": data["covered_total"],
        "ground_truth_total": data["ground_truth_total"],
        "percent": f"{data['percent']:.1f}" if data["percent"] is not None else "",
    })

with open(sys.argv[3], "w", newline="", encoding="utf-8") as handle:
    writer = csv.DictWriter(handle, fieldnames=list(rows[0]))
    writer.writeheader()
    writer.writerows(rows)
PY

cat <<MSG
[+] Comparison complete.
[+] Baseline AFL output:         $BASELINE_OUT
[+] Feedback-driven AFL output:  $FEEDBACK_OUT
[+] Baseline queue report:       $BASELINE_REPORT
[+] Feedback queue report:       $FEEDBACK_REPORT
[+] Summary CSV:                 $RUNS_DIR/reports/summary.csv
MSG
