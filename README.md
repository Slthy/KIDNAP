# KIDNAP

This repository contains AFL/AFL++ demonstration targets for experimenting with
feedback-aware fuzzing.

## Available demos

- [`demos/PoC_one`](demos/PoC_one/README.md): a bounded, non-destructive Linux
  syscall exerciser with optional `SYSCALL_FEEDBACK` instrumentation and helper
  scripts for fuzzing, one-command baseline-vs-feedback comparisons, queue
  analysis, and feedback-growth plotting.
- [`demos/PoC_two`](demos/PoC_two/README.md): a config parser simulator with a
  baseline build, a `CONFIGLET_FEEDBACK` build that feeds semantic
  feature/combo/dependency events back to AFL++, and comparison, analysis, and
  semantic-growth plotting helpers.

## Comparing fixed-duration runs

Each demo README documents a one-command baseline-vs-feedback helper. In short,
run the helper from the demo directory with a fixed duration, for example:

```sh
./scripts/run_feedback_comparison.sh -d 60
```

The helpers write side-by-side JSON reports, a compact `summary.csv`, and growth
CSV/PNG artifacts under `comparison-runs/reports/`. Growth plots use AFL queue
discovery timestamps, so a line ending before the chosen `-d` duration means the
last queued discovery happened early; it does not mean AFL stopped early. The
comparison helpers now extend plots to the selected duration to show the final
plateau clearly.

## Quick start

```sh
cd demos/PoC_one
make
printf '\x00\x00\x00\x00' > /tmp/poc_one.seed
./poc_one /tmp/poc_one.seed
./scripts/analyze_queue.py /tmp/poc_one.seed
```

See each PoC README for AFL++ fuzzing commands, experiment reset helpers, and
analysis examples.
