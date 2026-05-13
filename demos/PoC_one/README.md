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

## Fuzzing with AFL++

Install AFL++ and ensure `afl-fuzz` is on `PATH`. The helper scripts also
recognize common AFL++ binary names such as `afl-fuzz++`; if the fuzzer lives
elsewhere, set `AFL_FUZZ=/path/to/afl-fuzz`. If an AFL compiler wrapper such as
`afl-cc`, `afl-clang-fast`, or `afl-clang-lto` is present, the helper scripts
use it automatically; otherwise they build with the default compiler and print a
warning. Set `AFL_CC=/path/to/compiler-wrapper` to choose a specific AFL
compiler wrapper.

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

## Queue analysis

`analyze_queue.py` decodes AFL queue entries or standalone testcase files and
reports the syscall classes and two-syscall transitions represented by the
corpus.

Analyze a single testcase:

```sh
./scripts/analyze_queue.py /tmp/poc_one.seed
```

Analyze an AFL output directory and replay each queue entry through the current
binary as a smoke check:

```sh
./scripts/analyze_queue.py --target ./poc_one ./out-sysfeedback
```

Write machine-readable outputs:

```sh
./scripts/analyze_queue.py --format json --details --csv queue_summary.csv ./out-sysfeedback
```

## Plotting feedback growth

`plot_feedback.py` derives cumulative syscall and `SEQ2` growth from AFL queue
files. It can compare multiple output directories.

Create a PNG plot (requires `matplotlib`):

```sh
./scripts/plot_feedback.py ./out-baseline ./out-sysfeedback -o feedback_growth.png
```

Export the same time-series without requiring plotting dependencies:

```sh
./scripts/plot_feedback.py --no-plot --csv feedback_growth.csv ./out-baseline ./out-sysfeedback
```

## Expected workflow

1. Build and smoke-test with `make`.
2. Start a baseline fuzzing run with `scripts/run_baseline.sh`.
3. Start a syscall-feedback fuzzing run with `scripts/run_sysfeedback.sh`.
4. Compare the resulting queue corpora with `scripts/analyze_queue.py`.
5. Generate a CSV or PNG comparison with `scripts/plot_feedback.py`.
