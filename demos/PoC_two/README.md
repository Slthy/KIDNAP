# PoC 2: configlet simulator

PoC 2 is an AFL-friendly configuration parser experiment. The target parses
human-readable config strings and reports semantic feature, combination, and
dependency coverage through `CONFIGLET_FEEDBACK_FILE`.

The simulator is intentionally harder than a flat token target:

- The parser accepts exact `KEY=value` lines rather than arbitrary substring
  hits, so injected dictionary tokens need line structure.
- Mid-level features require several simultaneous settings instead of a single
  token.
- Deep features form a hierarchy where later unlocks depend on earlier workload
  combinations such as admin, export, backup, query acceleration, and failover.
- Numeric thresholds and magic values gate deeper states (`SIZE>2048`,
  `THREADS>=8`, `TIMEOUT>=100`, `TOKEN=1337`, `CHECKSUM=42`, and
  `MAGIC=0xdead`).
- `LARGE_WRITE_BACKUP` also requires a stateful transaction order:
  `AUTH=1`, `DB=mysql`, `BEGIN_TX=1`, `WRITE=1`, `COMMIT=1`, then `BACKUP=1`.

## Build modes

The default build is the baseline target. It keeps the semantic log available
for offline analysis, but it does not feed those semantic events back to AFL++:

```bash
make clean
make
```

The feedback-driven build defines `CONFIGLET_FEEDBACK`. In this mode every
semantic feature, combination, and dependency is also emitted as synthetic
AFL++ coverage with `__afl_coverage_interesting` when built by an AFL++ compiler
wrapper. Non-AFL builds use an internal fallback map so the target still links
for smoke tests:

```bash
make clean
make feedback
```

## Running individual campaigns

Run the baseline campaign:

```bash
./scripts/reset.sh --build
./scripts/run_baseline.sh
```

Run the feedback-driven campaign:

```bash
./scripts/run_feedback.sh
```

Both runners use:

- `seeds/` for meaningful but shallow starting configurations.
- `dict/configlet.dict` for AFL tokens such as `AUTH=1`, `DB=mysql`,
  `ROLE=admin`, and the deeper numeric/magic values.
- `CONFIGLET_FEEDBACK_FILE` for feature/combo/dependency logging.

After a campaign, inspect the generated corpus. The analyzer accepts either an
AFL output directory or the queue directory itself:

```bash
./scripts/analyze_queue.py out-baseline
./scripts/analyze_queue.py out-configlet-feedback
```

Use JSON output when you want to post-process results:

```bash
./scripts/analyze_queue.py --format json out-baseline > baseline.json
```

## Plotting semantic growth

Use `plot_feedback.py` to replay one or more AFL output directories and plot
cumulative semantic discovery over queue time. The plot contains features,
combinations, dependencies, and total covered ground-truth states:

```bash
./scripts/plot_feedback.py \
  --target ./configlet_sim \
  --output configlet_feedback_growth.png \
  out-baseline out-configlet-feedback
```

If `matplotlib` is not installed, or you want to analyze the data in another
tool, export the same time-series data as CSV without creating a PNG:

```bash
./scripts/plot_feedback.py \
  --target ./configlet_sim \
  --csv configlet_feedback_growth.csv \
  --no-plot \
  out-baseline out-configlet-feedback
```

The script accepts normal AFL output directories, instance directories, queue
directories, or simple testcase directories. AFL queue filenames with
`time:<milliseconds>` are plotted using AFL's fuzzing-relative timestamp; other
files fall back to modification-time deltas.

## Baseline vs feedback-driven comparison

For a repeatable side-by-side run, use the comparison helper. It builds separate
baseline and feedback-driven binaries, runs fixed-duration AFL++ campaigns with
the same seeds and dictionary, and writes JSON reports plus a compact CSV
summary:

```bash
./scripts/run_feedback_comparison.sh -d 60
```

Outputs are written under `comparison-runs/`:

- `comparison-runs/baseline/` and `comparison-runs/configlet-feedback/` contain
  the AFL++ campaign outputs.
- `comparison-runs/reports/baseline.json` contains the replayed semantic
  coverage report for the baseline queue.
- `comparison-runs/reports/configlet-feedback.json` contains the replayed
  semantic coverage report for the feedback-driven queue.
- `comparison-runs/reports/summary.csv` compares covered features,
  combinations, dependencies, and total ground-truth percentage.

To plot those comparison outputs after the run, pass both campaign directories
to the plotting helper:

```bash
./scripts/plot_feedback.py \
  --target comparison-runs/bin/configlet_feedback \
  --output comparison-runs/reports/configlet_feedback_growth.png \
  --csv comparison-runs/reports/configlet_feedback_growth.csv \
  comparison-runs/baseline comparison-runs/configlet-feedback
```

## Fuzzing guidance

The dictionary still contains valid config vocabulary, but individual tokens no
longer flatten the target. Useful discoveries generally require structured
multi-line inputs, numeric mutation, and preserving order for the transaction
sequence.

Examples of the deeper semantic paths:

- `PRIVILEGED_ADMIN` requires the `ADMIN` hierarchy plus `TOKEN=1337`,
  `MODE=write`, and `CHECKSUM=42`.
- `LARGE_EXPORT` requires `EXPORT=1`, `COMPRESS=gzip`, `FORMAT=json`,
  `SIZE>2048`, and `MODE=write`.
- `QUERY_ACCEL` requires `CACHE=1`, `DB=mysql`, `THREADS>=8`, and `CHECKSUM=42`.
- `FAILOVER` requires `TLS=1`, `DB=mysql`, `REGION=us-east`, `TIMEOUT>=100`,
  and `CHECKSUM=42`.
- `CLUSTER_SYNC` requires both `FAILOVER` and `QUERY_ACCEL`, plus
  `MAGIC=0xdead`.
- `RECOVERY` requires `CLUSTER_SYNC`, `LARGE_WRITE_BACKUP`, `LARGE_EXPORT`, and
  `PRIVILEGED_ADMIN` in the same input.
