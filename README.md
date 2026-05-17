# KIDNAP

This repository currently contains `PoC_one`, an AFL/AFL++ demonstration target
for experimenting with syscall-oriented feedback.

## Available demos

- [`demos/PoC_one`](demos/PoC_one/README.md): a bounded, non-destructive Linux
  syscall exerciser with optional `SYSCALL_FEEDBACK` instrumentation and helper
  scripts for fuzzing, one-command baseline-vs-feedback comparisons, queue
  analysis, and feedback-growth plotting.

## Quick start

```sh
cd demos/PoC_one
make
printf '\x00\x00\x00\x00' > /tmp/poc_one.seed
./poc_one /tmp/poc_one.seed
./scripts/analyze_queue.py /tmp/poc_one.seed
```

See the PoC README for AFL++ fuzzing commands, experiment reset helpers, and
plotting examples.
