# PoC one: AFL-aware safe syscall exerciser

This target decodes the input as a stream of four-byte operations:

```text
syscall_id arg0 arg1 arg2
```

Only the first `MAX_OPS` operations are executed. Each operation maps to a small allowlist of non-destructive syscalls (`openat`, `read`, `write`, `close`, `fstat`, `mmap`, `mprotect`, `socket`, `bind`, `setsockopt`, `getpid`, and `uname`). File operations are constrained to fixed paths under `/tmp/afl-syscall-poc/`, and read/write lengths are capped by `MAX_RW_SIZE`.

Build the normal target:

```sh
make
```

Build the AFL syscall-feedback target:

```sh
make sysfeedback
```

When `SYSCALL_FEEDBACK` is enabled, the target writes synthetic features into AFL's coverage map in addition to normal compiler-inserted edge coverage:

- `SYSCALL(syscall_nr)` for each syscall class reached.
- `ERRNO(syscall_nr, errno)` for each failing syscall result.
- `SEQ2(previous_syscall_nr, syscall_nr)` for short syscall-order feedback.

A 500 ms process timeout is installed as a standalone safety net. AFL can still use its own `-t` timeout setting when fuzzing.
