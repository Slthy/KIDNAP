# PoC 2: configlet simulator

PoC 2 is an AFL-friendly configuration parser experiment. The target still parses
human-readable config strings, but it now gives AFL more structure and more branch
variation to discover.

## Running the baseline

```bash
./scripts/reset.sh --build
./scripts/run_baseline.sh
```

The baseline runner uses:

- `seeds/` for multiple meaningful starting configurations.
- `dict/configlet.dict` for AFL tokens such as `AUTH=1`, `DB=mysql`,
  `ROLE=admin`, and the deeper magic values.
- `CONFIGLET_FEEDBACK_FILE=feedback-baseline.log` for feature/combo/dependency
  logging.

After a campaign, inspect the generated corpus:

```bash
./scripts/analyze_queue.py out-baseline/default/queue
```

## Fuzzing guidance

The parser intentionally relies on exact strings such as `AUTH=1`, `DB=mysql`,
`ROLE=admin`, `TOKEN=1337`, `SIZE=4096`, and `MODE=write`. Run AFL with the
included dictionary so mutation stages can assemble valid config lines instead
of waiting for random byte flips to invent them.

The target also includes nested branches inside features and two deeper semantic
paths:

- `PRIVILEGED_ADMIN` requires `AUTH=1`, `ROLE=admin`, and `TOKEN=1337`.
- `LARGE_WRITE_BACKUP` requires `BACKUP=1`, `SIZE=4096`, and `MODE=write`.
