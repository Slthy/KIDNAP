#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# shellcheck source=afl_common.sh
source "$ROOT_DIR/scripts/afl_common.sh"

usage() {
    cat <<'MSG'
usage: run_tmux_campaigns.sh [options]

Start independent baseline and syscall-feedback AFL++ campaigns in a tmux
session. The script builds stable target copies first, then launches fuzzers
against those copies so many tmux jobs do not race through make clean/build.

Options:
  -i DIR   seed input directory (default: ./in)
  -o DIR   runs output directory (default: ./runs)
  -b N     number of baseline campaigns (default: 10)
  -f N     number of syscall-feedback campaigns (default: 10)
  -s NAME  tmux session name (default: kidnap-poc-one)
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
RUNS_DIR="$ROOT_DIR/runs"
BASELINE_COUNT=10
FEEDBACK_COUNT=10
SESSION="kidnap-poc-one"
TIMEOUT_MS=1000

while getopts ':i:o:b:f:s:t:h' opt; do
    case "$opt" in
        i) IN_DIR="$OPTARG" ;;
        o) RUNS_DIR="$OPTARG" ;;
        b) BASELINE_COUNT="$OPTARG" ;;
        f) FEEDBACK_COUNT="$OPTARG" ;;
        s) SESSION="$OPTARG" ;;
        t) TIMEOUT_MS="$OPTARG" ;;
        h) usage; exit 0 ;;
        :) echo "error: -$OPTARG requires an argument" >&2; usage >&2; exit 2 ;;
        \?) echo "error: unknown option -$OPTARG" >&2; usage >&2; exit 2 ;;
    esac
done

if ! [[ "$BASELINE_COUNT" =~ ^[0-9]+$ && "$FEEDBACK_COUNT" =~ ^[0-9]+$ && "$TIMEOUT_MS" =~ ^[0-9]+$ ]]; then
    echo 'error: -b, -f, and -t must be non-negative integers' >&2
    exit 2
fi

if ! command -v tmux >/dev/null 2>&1; then
    echo 'error: tmux is not on PATH' >&2
    exit 127
fi

AFL_FUZZ_BIN="$(resolve_afl_fuzz)"

mkdir -p "$IN_DIR" "$RUNS_DIR" "$RUNS_DIR/logs" "$RUNS_DIR/bin" "$RUNS_DIR/.tmux_commands"
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

BASELINE_BIN="$RUNS_DIR/bin/poc_one_baseline"
FEEDBACK_BIN="$RUNS_DIR/bin/poc_one_sysfeedback"

echo "[*] Building baseline target copy: $BASELINE_BIN"
build_target_copy '' "$BASELINE_BIN"
echo "[*] Building syscall-feedback target copy: $FEEDBACK_BIN"
build_target_copy sysfeedback "$FEEDBACK_BIN"

if tmux has-session -t "$SESSION" 2>/dev/null; then
    echo "error: tmux session '$SESSION' already exists; attach with: tmux attach -t $SESSION" >&2
    exit 1
fi

tmux new-session -d -s "$SESSION" -c "$ROOT_DIR" -n controller
tmux set-option -t "$SESSION" remain-on-exit on >/dev/null

tmux send-keys -t "$SESSION:controller" \
    "printf 'KIDNAP AFL campaigns started. Attach with: tmux attach -t %q\\n' '$SESSION'; printf 'Outputs: %q\\n' '$RUNS_DIR';" C-m

write_run_command() {
    local kind="$1"
    local idx="$2"
    local target_bin="$3"
    local out_dir="$4"
    local log_file="$5"
    local cmd_file="$RUNS_DIR/.tmux_commands/${kind}_${idx}.sh"
    local skip_cpufreq="${AFL_SKIP_CPUFREQ:-1}"
    local dont_care_crashes="${AFL_I_DONT_CARE_ABOUT_MISSING_CRASHES:-1}"

    cat > "$cmd_file" <<CMD
#!/usr/bin/env bash
set -euo pipefail
cd "$ROOT_DIR"
mkdir -p "$out_dir" "$(dirname "$log_file")"
export AFL_SKIP_CPUFREQ="$skip_cpufreq"
export AFL_I_DONT_CARE_ABOUT_MISSING_CRASHES="$dont_care_crashes"
env -u AFL_FUZZ "$AFL_FUZZ_BIN" -i "$IN_DIR" -o "$out_dir" -t "$TIMEOUT_MS" -- "$target_bin" @@ 2>&1 | tee "$log_file"
CMD
    chmod +x "$cmd_file"
    printf '%s\n' "$cmd_file"
}

for ((i = 1; i <= BASELINE_COUNT; i++)); do
    out_dir="$RUNS_DIR/base_$i"
    log_file="$RUNS_DIR/logs/base_$i.log"
    cmd_file="$(write_run_command base "$i" "$BASELINE_BIN" "$out_dir" "$log_file")"
    tmux new-window -t "$SESSION" -c "$ROOT_DIR" -n "base_$i" "bash '$cmd_file'"
done

for ((i = 1; i <= FEEDBACK_COUNT; i++)); do
    out_dir="$RUNS_DIR/fb_$i"
    log_file="$RUNS_DIR/logs/fb_$i.log"
    cmd_file="$(write_run_command fb "$i" "$FEEDBACK_BIN" "$out_dir" "$log_file")"
    tmux new-window -t "$SESSION" -c "$ROOT_DIR" -n "fb_$i" "bash '$cmd_file'"
done

cat <<MSG
[+] Started tmux session: $SESSION
[+] Attach: tmux attach -t $SESSION
[+] Stop all fuzzers: tmux kill-session -t $SESSION
[+] Stop and reset outputs: ./scripts/reset_experiments.sh --session $SESSION --runs '$RUNS_DIR'
[+] Outputs: $RUNS_DIR/base_* and $RUNS_DIR/fb_*
[+] Logs: $RUNS_DIR/logs/*.log
MSG
