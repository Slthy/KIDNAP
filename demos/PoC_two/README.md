# PoC 2: configlet simulator

PoC 2 is an AFL-friendly configuration parser experiment. The target parses
human-readable config strings and reports semantic feature, combination, and
dependency coverage through `CONFIGLET_FEEDBACK_FILE`.

The simulator is intentionally harder than a flat token target:

- The parser now accepts exact `KEY=value` lines rather than arbitrary substring
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

## Running the baseline

```bash
./scripts/reset.sh --build
./scripts/run_baseline.sh
```

The baseline runner uses:

- `seeds/` for meaningful but shallow starting configurations.
- `dict/configlet.dict` for AFL tokens such as `AUTH=1`, `DB=mysql`,
  `ROLE=admin`, and the deeper numeric/magic values.
- `CONFIGLET_FEEDBACK_FILE=feedback-baseline.log` for feature/combo/dependency
  logging.

After a campaign, inspect the generated corpus:

```bash
./scripts/analyze_queue.py out-baseline/default/queue
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
- `QUERY_ACCEL` requires `CACHE=1`, `DB=mysql`, `THREADS>=8`, and
  `CHECKSUM=42`.
- `FAILOVER` requires `TLS=1`, `DB=mysql`, `REGION=us-east`, `TIMEOUT>=100`,
  and `CHECKSUM=42`.
- `CLUSTER_SYNC` requires both `FAILOVER` and `QUERY_ACCEL`, plus
  `MAGIC=0xdead`.
- `RECOVERY` requires `CLUSTER_SYNC`, `LARGE_WRITE_BACKUP`, `LARGE_EXPORT`,
  and `PRIVILEGED_ADMIN` in the same input.
