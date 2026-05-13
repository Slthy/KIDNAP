#!/usr/bin/env bash

resolve_afl_fuzz() {
    if [[ -n "${AFL_FUZZ:-}" ]]; then
        if command -v "$AFL_FUZZ" >/dev/null 2>&1; then
            command -v "$AFL_FUZZ"
            return 0
        fi
        if [[ -x "$AFL_FUZZ" ]]; then
            printf '%s\n' "$AFL_FUZZ"
            return 0
        fi
        echo "error: AFL_FUZZ is set to '$AFL_FUZZ', but it is not executable or on PATH" >&2
        return 127
    fi

    local candidate
    for candidate in afl-fuzz afl-fuzz++ afl-fuzz-fast; do
        if command -v "$candidate" >/dev/null 2>&1; then
            command -v "$candidate"
            return 0
        fi
    done

    cat >&2 <<'MSG'
error: afl-fuzz is not on PATH
Install AFL++ and ensure its binaries are on PATH, or set AFL_FUZZ to the afl-fuzz executable path.
Examples:
  sudo apt-get install afl++
  AFL_FUZZ=/path/to/afl-fuzz ./scripts/run_baseline.sh in out-baseline
MSG
    return 127
}

resolve_afl_cc() {
    if [[ -n "${AFL_CC:-}" ]]; then
        if command -v "$AFL_CC" >/dev/null 2>&1; then
            command -v "$AFL_CC"
            return 0
        fi
        if [[ -x "$AFL_CC" ]]; then
            printf '%s\n' "$AFL_CC"
            return 0
        fi
        echo "warning: AFL_CC is set to '$AFL_CC', but it is not executable or on PATH" >&2
        return 1
    fi

    local candidate
    for candidate in afl-cc afl-clang-fast afl-clang-lto afl-gcc-fast; do
        if command -v "$candidate" >/dev/null 2>&1; then
            command -v "$candidate"
            return 0
        fi
    done

    return 1
}
