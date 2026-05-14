# PoC one: AFL-aware safe syscall exerciser

`PoC_one` is a small Linux fuzzing target for comparing plain AFL edge coverage
with extra syscall-oriented feedback. It decodes each input file as a bounded
stream of four-byte operations and executes only an allowlisted set of
non-destructive syscalls.

## Input format

Each operation is exactly four bytes:

```text
syscall_id arg0 arg1 arg2
```

Only the first `MAX_OPS` operations are executed (`MAX_OPS` is 64). The syscall
is selected with `syscall_id % OP_MAX`; the remaining bytes are interpreted as
bounded selectors, sizes, or option values depending on the syscall. Partial
trailing operations are ignored.

## Safety model

The exerciser intentionally keeps side effects small and local:

- The syscall allowlist is `openat`, `read`, `write`, `close`, `fstat`, `mmap`,
  `mprotect`, `socket`, `bind`, `setsockopt`, `getpid`, and `uname`.
- File operations are constrained to `/tmp/afl-syscall-poc/`.
- Read and write lengths are capped by `MAX_RW_SIZE` (4096 bytes).
- Opened file descriptors, sockets, and anonymous mappings are closed or
  unmapped immediately.
- A 500 ms process timer is installed as a standalone safety net; AFL can still
  apply its own `-t` timeout.

## Build and smoke-test

From this directory:

```sh
make clean
make
printf '\x00\x00\x00\x00\x07\x00\x00\x00' > /tmp/poc_one.seed
./poc_one /tmp/poc_one.seed
```

Build the syscall-feedback variant with:

```sh
make clean
make sysfeedback
./poc_one /tmp/poc_one.seed
```

When `SYSCALL_FEEDBACK` is enabled, the target writes synthetic features into
AFL's coverage map in addition to normal compiler-inserted edge coverage:

- `SYSCALL(syscall_nr)` for each syscall class reached.
- `ERRNO(syscall_nr, errno)` for each failing syscall result.
- `SEQ2(previous_syscall_nr, syscall_nr)` for short syscall-order feedback.
- `SEQ3(older_syscall_nr, previous_syscall_nr, syscall_nr)` for a harder
  syscall-order signal that is less likely to saturate during short campaigns.

## Fuzzing with AFL++

Install AFL++ and ensure `afl-fuzz` is on `PATH`. The helper scripts also
recognize common AFL++ binary names such as `afl-fuzz++`; if the fuzzer lives
elsewhere, set `AFL_FUZZ=/path/to/afl-fuzz`. The scripts consume this helper
variable and unset it before launching AFL++ so AFL++ does not report it as a
mistyped `AFL_*` environment variable. If an AFL compiler wrapper such as
`afl-cc`, `afl-clang-fast`, or `afl-clang-lto` is present, the helper scripts
use it automatically; otherwise they build with the default compiler and print a
warning. Set `AFL_CC=/path/to/compiler-wrapper` to choose a specific AFL
compiler wrapper.

On Linux hosts where `/proc/sys/kernel/core_pattern` pipes crashes to an
external handler, AFL++ may abort before fuzzing starts. For these demo runs the
helper scripts automatically set `AFL_I_DONT_CARE_ABOUT_MISSING_CRASHES=1` in
that situation and print a warning. For precise crash triage, prefer changing
the host setting temporarily with `echo core | sudo tee /proc/sys/kernel/core_pattern`.

Run the baseline target:

```sh
./scripts/run_baseline.sh [seed_dir] [out_dir]
```

Run the syscall-feedback target:

```sh
./scripts/run_sysfeedback.sh [seed_dir] [out_dir]
```

If `seed_dir` is omitted, the scripts create `./in/seed`. If `out_dir` is
omitted, baseline results go to `./out-baseline` and syscall-feedback results go
to `./out-sysfeedback`.

For larger machines, start multiple independent campaigns in `tmux` after one
serial build of stable baseline and syscall-feedback target copies:

```sh
./scripts/run_tmux_campaigns.sh -i ./in -o ./runs -b 10 -f 10 -s kidnap-poc-one
tmux attach -t kidnap-poc-one
```

The tmux helper defaults to 10 baseline plus 10 syscall-feedback campaigns, sets
`AFL_SKIP_CPUFREQ=1` and `AFL_I_DONT_CARE_ABOUT_MISSING_CRASHES=1` for each
window unless those variables are already set, writes outputs under
`runs/base_*` and `runs/fb_*`, and writes logs under `runs/logs/`. Stop all runs
with `tmux kill-session -t kidnap-poc-one`.

## Resetting experiments

Use `reset_experiments.sh` after stopping a fuzzing run, or when you want to
throw away generated AFL queues and restart from scratch:

```sh
./scripts/reset_experiments.sh
```

By default the reset helper kills the `kidnap-poc-one` tmux session if it is
running, terminates direct `afl-fuzz` processes whose command line references
this demo directory, and removes generated output directories:
`./out-baseline`, `./out-sysfeedback`, and `./runs`. It preserves the seed input
folder `./in`; pass `--delete-input` if you also want to recreate seeds on the
next run.

Useful variants:

```sh
./scripts/reset_experiments.sh --dry-run
./scripts/reset_experiments.sh --stop-only
./scripts/reset_experiments.sh --clean-only ./custom-afl-out
```

## Queue analysis

`analyze_queue.py` decodes AFL queue entries or standalone testcase files and
reports syscall classes, two-syscall transitions, three-syscall transitions, and
replay-derived errno combinations represented by the corpus. The errno metrics
require `--target`; the helper enables `KIDNAP_TRACE_SYSCALLS=1` while replaying
so the target emits one compact trace line per executed operation.

Analyze a single testcase:

```sh
./scripts/analyze_queue.py /tmp/poc_one.seed
```

Analyze an AFL++ default single-instance output directory and replay each queue
entry through the current binary as a smoke check. AFL++ stores those queue
files under `out-dir/default/queue/`, so pass the instance directory or the
queue directory directly:

```sh
./scripts/analyze_queue.py --target ./poc_one ./out-sysfeedback/default
```

Direct queue paths are accepted too:

```sh
./scripts/analyze_queue.py --target ./poc_one ./out-sysfeedback/default/queue
```

Write machine-readable outputs:

```sh
./scripts/analyze_queue.py --format json --details --csv queue_summary.csv ./out-sysfeedback/default
```

## Plotting feedback growth

`plot_feedback.py` derives cumulative syscall, `SEQ2`, and `SEQ3` growth from
AFL queue files and reports when each metric first reaches its final value for
that corpus as a time-to-saturation proxy. It can compare multiple output
directories. For AFL++ default
single-instance runs, pass the instance directories (`out-baseline/default` and
`out-sysfeedback/default`) because AFL++ stores queue files under
`out-dir/default/queue/`, not directly under `out-dir/queue/`.

Create a PNG plot (requires `matplotlib`):

```sh
./scripts/plot_feedback.py ./out-baseline/default ./out-sysfeedback/default -o feedback_growth.png
```

Export the same time-series without requiring plotting dependencies:

```sh
./scripts/plot_feedback.py --no-plot --csv feedback_growth.csv ./out-baseline/default ./out-sysfeedback/default
```

## Expected workflow

1. Build and smoke-test with `make`.
2. Start a baseline fuzzing run with `scripts/run_baseline.sh`.
3. Start a syscall-feedback fuzzing run with `scripts/run_sysfeedback.sh`.
4. Compare the resulting queue corpora with `scripts/analyze_queue.py`.
5. Generate a CSV or PNG comparison with `scripts/plot_feedback.py`.
