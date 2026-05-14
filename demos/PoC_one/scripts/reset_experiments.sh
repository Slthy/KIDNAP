#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SESSION="kidnap-poc-one"
RUNS_DIR="$ROOT_DIR/runs"
OUT_DIRS=("$ROOT_DIR/out-baseline" "$ROOT_DIR/out-sysfeedback")
EXTRA_PATHS=()
DELETE_INPUT=0
STOP_ONLY=0
CLEAN_ONLY=0
DRY_RUN=0
QUIET=0

usage() {
    cat <<'MSG'
usage: reset_experiments.sh [options] [path ...]

Stop PoC_one AFL++ experiments and remove generated AFL output so the next run
starts from a clean slate. By default the script:
  * kills the default tmux session if it exists;
  * asks AFL++ processes that are fuzzing this PoC directory to stop;
  * removes ./out-baseline, ./out-sysfeedback, and ./runs.

Extra path arguments are also removed, but only when they live inside this demo
directory. The seed input directory ./in is preserved unless --delete-input is
provided.

Options:
  -s NAME, --session NAME   tmux session to kill (default: kidnap-poc-one)
  -o DIR,  --out DIR        generated AFL output directory to delete; can repeat
  -r DIR,  --runs DIR       tmux campaign directory to delete (default: ./runs)
          --delete-input    also delete ./in
          --stop-only       only stop running fuzzers; do not delete files
          --clean-only      only delete files; do not stop running fuzzers
  -n,     --dry-run         print what would be done without changing anything
  -q,     --quiet           reduce informational output
  -h,     --help            show this help

Examples:
  ./scripts/reset_experiments.sh
  ./scripts/reset_experiments.sh --dry-run
  ./scripts/reset_experiments.sh --session kidnap-poc-one --delete-input
  ./scripts/reset_experiments.sh ./custom-out ./more-runs
MSG
}

log() {
    if [[ "$QUIET" -eq 0 ]]; then
        printf '%s\n' "$*"
    fi
}

run_cmd() {
    if [[ "$DRY_RUN" -eq 1 ]]; then
        printf '[dry-run] '
        printf '%q ' "$@"
        printf '\n'
        return 0
    fi
    "$@"
}

abs_path() {
    local path="$1"
    if [[ -d "$path" ]]; then
        (cd "$path" && pwd -P)
    else
        local dir base
        dir="$(dirname "$path")"
        base="$(basename "$path")"
        (cd "$dir" 2>/dev/null && printf '%s/%s\n' "$(pwd -P)" "$base") || return 1
    fi
}

require_demo_path() {
    local path="$1"
    local abs root_abs
    abs="$(abs_path "$path")" || {
        echo "warning: skipping '$path' because its parent does not exist" >&2
        return 1
    }
    root_abs="$(abs_path "$ROOT_DIR")"

    case "$abs" in
        "$root_abs"/*) printf '%s\n' "$abs" ;;
        *)
            echo "warning: refusing to remove '$path' because it is outside $ROOT_DIR" >&2
            return 1
            ;;
    esac
}

add_out_dir() {
    OUT_DIRS+=("$1")
}

while (($#)); do
    case "$1" in
        -s|--session)
            [[ $# -ge 2 ]] || { echo 'error: --session requires an argument' >&2; exit 2; }
            SESSION="$2"
            shift 2
            ;;
        -o|--out)
            [[ $# -ge 2 ]] || { echo 'error: --out requires an argument' >&2; exit 2; }
            add_out_dir "$2"
            shift 2
            ;;
        -r|--runs)
            [[ $# -ge 2 ]] || { echo 'error: --runs requires an argument' >&2; exit 2; }
            RUNS_DIR="$2"
            shift 2
            ;;
        --delete-input)
            DELETE_INPUT=1
            shift
            ;;
        --stop-only)
            STOP_ONLY=1
            shift
            ;;
        --clean-only)
            CLEAN_ONLY=1
            shift
            ;;
        -n|--dry-run)
            DRY_RUN=1
            shift
            ;;
        -q|--quiet)
            QUIET=1
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        --)
            shift
            while (($#)); do
                EXTRA_PATHS+=("$1")
                shift
            done
            ;;
        -*)
            echo "error: unknown option '$1'" >&2
            usage >&2
            exit 2
            ;;
        *)
            EXTRA_PATHS+=("$1")
            shift
            ;;
    esac
done

if [[ "$STOP_ONLY" -eq 1 && "$CLEAN_ONLY" -eq 1 ]]; then
    echo 'error: --stop-only and --clean-only cannot be used together' >&2
    exit 2
fi

stop_tmux_session() {
    if ! command -v tmux >/dev/null 2>&1; then
        log '[*] tmux is not on PATH; skipping tmux cleanup'
        return 0
    fi

    if tmux has-session -t "$SESSION" 2>/dev/null; then
        log "[*] Killing tmux session: $SESSION"
        run_cmd tmux kill-session -t "$SESSION"
    else
        log "[*] No tmux session named '$SESSION' is running"
    fi
}

stop_afl_processes() {
    if ! command -v pgrep >/dev/null 2>&1; then
        log '[*] pgrep is not on PATH; skipping direct AFL process cleanup'
        return 0
    fi

    local pattern pids
    pattern="afl-fuzz.*($ROOT_DIR|$ROOT_DIR/poc_one|$ROOT_DIR/runs/bin)"
    pids="$(pgrep -f "$pattern" || true)"
    if [[ -z "$pids" ]]; then
        log '[*] No direct AFL++ fuzzers for this demo appear to be running'
        return 0
    fi

    log "[*] Stopping AFL++ fuzzers for this demo: ${pids//$'\n'/ }"
    if [[ "$DRY_RUN" -eq 1 ]]; then
        while IFS= read -r pid; do
            [[ -n "$pid" ]] && printf '[dry-run] kill -TERM %q\n' "$pid"
        done <<< "$pids"
        return 0
    fi

    while IFS= read -r pid; do
        [[ -n "$pid" ]] && kill -TERM "$pid" 2>/dev/null || true
    done <<< "$pids"

    sleep 1

    while IFS= read -r pid; do
        if [[ -n "$pid" ]] && kill -0 "$pid" 2>/dev/null; then
            kill -KILL "$pid" 2>/dev/null || true
        fi
    done <<< "$pids"
}

collect_cleanup_paths() {
    local paths=()
    paths+=("${OUT_DIRS[@]}" "$RUNS_DIR")
    if [[ "$DELETE_INPUT" -eq 1 ]]; then
        paths+=("$ROOT_DIR/in")
    fi
    paths+=("${EXTRA_PATHS[@]}")
    printf '%s\n' "${paths[@]}"
}

clean_generated_paths() {
    local path safe_path seen_file
    seen_file="$(mktemp)"
    trap 'rm -f "$seen_file"' RETURN

    while IFS= read -r path; do
        [[ -n "$path" ]] || continue
        safe_path="$(require_demo_path "$path")" || continue
        if grep -Fxq "$safe_path" "$seen_file"; then
            continue
        fi
        printf '%s\n' "$safe_path" >> "$seen_file"

        if [[ -e "$safe_path" || -L "$safe_path" ]]; then
            log "[*] Removing: $safe_path"
            run_cmd rm -rf -- "$safe_path"
        else
            log "[*] Already clean: $safe_path"
        fi
    done < <(collect_cleanup_paths)
}

if [[ "$CLEAN_ONLY" -eq 0 ]]; then
    stop_tmux_session
    stop_afl_processes
fi

if [[ "$STOP_ONLY" -eq 0 ]]; then
    clean_generated_paths
fi

log '[+] Experiment reset complete'
