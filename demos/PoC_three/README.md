# PoC three: systemctl + ftrace workload expansion

`PoC_three` compares a hand-written `systemctl` workload with AFL-generated
command sequences and ftrace-selected corpora. In environments without nested
KVM, the intended stack is an **outer VM plus a disposable `systemd-nspawn`
container**: the harnesses and fake units run inside nspawn, while ftrace stays
on the outer VM because both layers share the same kernel.

The PoC’s question is deliberately narrower than “can AFL crash systemctl?”:

```text
Can generated systemctl workloads expose broader ftrace-observed kernel behavior
than a small manual suite?
```

## What is implemented

- A fast AFL-facing `systemctl_generator` that decodes four-byte records and
  emits semantic coverage for operations, repetitions, families, and adjacent
  operation pairs without invoking `systemctl`.
- A slower replay-only `systemctl_harness` that maps the same records onto a
  strict allowlist of real `systemctl` commands inside nspawn.
- Controlled unit files for success, failure, sleep, filesystem, timer, socket,
  and path activation cases.
- A baseline workload seed, AFL seeds, trace collection, trace summarization,
  queue replay/scoring, ftrace-selected corpus extraction, a reseeding loop, and
  a simple cumulative-growth plotter.
- A second fuzzing target that generates controlled `fuzz-generated.service`
  files from byte inputs using only allowlisted unit-file fields.
- Dry-run modes so both generators can be tested locally without invoking
  `systemctl`.
- `systemd-nspawn` helpers for building a disposable rootfs, booting the machine,
  running AFL inside it, and replaying individual inputs from the host while
  tracing the shared kernel.

This first cut intentionally excludes reboot/poweroff, rescue/emergency,
`isolate`, `kill`, arbitrary links/edits, and real-service mutation.

## Layout

```text
PoC_three/
├── harness/          safe byte parser and systemctl allowlist
├── units/            controlled unit files installed only inside nspawn
├── seeds/            binary AFL seeds for command-sequence fuzzing
├── unit-seeds/       binary AFL seeds for generated unit-file fuzzing
├── scripts/          nspawn setup, tracing, replay, reseeding, and plotting helpers
└── results/          generated traces and reports
```

## Input model

Each operation is four bytes:

```text
byte 0: allowlisted operation id
byte 1: reserved selector
byte 2: reserved flags
byte 3: repetition selector, capped to 1..3 runs
```

The harness processes at most 64 complete operations and ignores trailing
partial bytes. It never executes raw shell text from the testcase. Expected
`systemctl` failures are tolerated by default so error-path workloads remain
valid fuzz cases; set `SYSTEMCTL_HARNESS_STRICT=1` when you want a nonzero exit
for debugging.

Inspect what an input would do without touching systemd:

```bash
make
./systemctl_harness --dry-run seeds/baseline
```

The architecture is intentionally split:

```text
AFL generation:      systemctl_generator   (fast, semantic feedback only)
ftrace replay:       systemctl_harness     (slow, real systemctl effects)
```

This avoids spending most AFL cycles on D-Bus and service lifecycle overhead
while preserving real kernel-behavior measurement during replay.

## Recommended environment: outer VM + systemd-nspawn

Nested KVM is not required. On the outer VM:

```bash
make
sudo ./scripts/setup_nspawn.sh
```

Start the nspawn machine in one terminal:

```bash
sudo ./scripts/start_nspawn.sh
```

### If bootstrap fails on DNS or `afl++`

If an earlier `setup_nspawn.sh` run failed with messages like:

```text
Temporary failure resolving 'archive.ubuntu.com'
Unable to locate package afl++
```

rerun the setup helper after pulling this version:

```bash
sudo ./scripts/setup_nspawn.sh
```

The helper now uses nspawn's uplink resolver instead of the host stub resolver
and enables Ubuntu's `universe` component, which is where Jammy ships `afl++`.
It is safe to rerun against an already-created rootfs.

In a second outer-VM terminal, configure host-side tracing and install the fake
units inside the container:

```bash
sudo ./scripts/configure_ftrace_filters.sh
./scripts/install_units_nspawn.sh
```

Then run the baseline from the outer VM:

```bash
./scripts/run_baseline.sh
```

The script escalates only the ftrace collection step with `sudo`; the report
files remain owned by your normal user.

What happens here is intentionally split:

```text
outer VM:       starts/stops ftrace and analyzes traces
nspawn machine: runs systemd, fake units, and the harness binaries
```

Because nspawn shares the outer VM kernel, tracing is still global and can include
noise from unrelated host activity. The helper now defaults to a focused shortlist of low-overhead `syscalls`
tracepoints for routine replay rather than every syscall on the host. Function tracing remains available as
`TRACE_PROFILE=focused` or `TRACE_PROFILE=broad`, but on this host it is expensive
enough to reserve for a tiny second-pass corpus rather than every generated input.

The baseline trace/report land under `results/baseline/`. Queue analysis replays
each AFL testcase under host ftrace and emits a novelty-oriented JSON report plus
an optional CSV.

Before the first AFL run, build the binaries with AFL instrumentation inside
nspawn:

```bash
./scripts/build_nspawn.sh
```

Run AFL inside the nspawn machine, writing outputs back through the bind mount:

```bash
./scripts/run_afl_nspawn.sh
```

The AFL runners default `AFL_I_DONT_CARE_ABOUT_MISSING_CRASHES=1` because this
PoC is about workload generation, not crash triage, and nspawn shares the outer
VM's `core_pattern` setting. If you later switch to crash-focused experiments,
remove that override and configure host crash handling deliberately.

To turn that report into a ftrace-selected corpus from the outer VM:

```bash
./scripts/run_ftrace_cycle.sh out-afl results/ftrace-cycle
```

That writes `queue-growth.csv`, `queue-analysis.json`, and `seeds-next/` with the
highest-scoring inputs. To demonstrate a full reseeding experiment across fixed
AFL windows:

```bash
ROUNDS=3 DURATION=300 TOP_K=20 ./scripts/run_reseed_loop.sh
```

Each round runs AFL inside nspawn for `DURATION` seconds, resets container state
between replayed inputs, replays the resulting queue under outer-VM ftrace,
selects the most novel inputs, and uses only those as the next round’s seed
corpus. The replay path defaults to `scripts/nspawn_harness.sh`; set
`NSPAWN_TARGET_HARNESS=unit_file_harness` when replaying the unit-file family.

### One-command experiment matrix

Once nspawn is running and units are installed, you can execute the full
comparison matrix with:

```bash
DURATION=300 ROUNDS=3 TOP_K=20 PRESELECT_LIMIT=64 ./scripts/run_experiment_matrix.sh
```

It runs:

```text
manual baseline
command-sequence AFL
command-sequence AFL + ftrace reseeding
unit-file AFL
unit-file AFL + ftrace reseeding
```

Outputs are grouped under `results/experiment-matrix/` by default. The compact
reporting artifacts are:

```text
reports/summary.csv
reports/summary.json
reports/function-growth-comparison.png   # when matplotlib is installed
```

The summary normalizes the unlike raw outputs into one table containing unique
functions, unique function pairs, subsystem counts, replayed queue size, and
aggregate novelty score.

## Generated unit-file fuzzing

`unit_file_harness` maps the first eight input bytes onto a controlled service
file:

```text
Type=              oneshot | simple
ExecStart=         /bin/true | /bin/false | /bin/sleep 1 | safe /tmp writer
Restart=           no | on-failure | always
RemainAfterExit=   no | yes
PrivateTmp=        no | yes
NoNewPrivileges=   no | yes
WorkingDirectory=  /tmp | /tmp/systemctl-fuzz
RuntimeDirectory=  absent | systemctl-fuzz-runtime
```

It writes only `fuzz-generated.service` under `UNIT_FILE_DIR` (default:
`/run/systemd/system`) and then runs the fixed sequence `daemon-reload`,
`start`, `status`, `stop`, `reset-failed`. Inspect a generated unit locally with:

```bash
./unit_file_harness --dry-run unit-seeds/private-tmp
```

Run its AFL family inside nspawn:

```bash
./scripts/run_unitfile_afl_nspawn.sh
```

Reuse the same host-traced reseeding loop for generated unit files with:

```bash
NSPAWN_TARGET_HARNESS=unit_file_harness \
  FUZZ_RUNNER=./scripts/run_unitfile_afl_nspawn.sh \
  INITIAL_SEEDS=./unit-seeds \
  ./scripts/run_reseed_loop.sh results/unitfile-reseed-loop
```

Keeping this as a second target makes it possible to compare command-sequence
fuzzing and unit-file fuzzing as distinct workload families.

## Novelty signals

`analyze_trace.py` can parse both function-tracer output and syscall tracepoint
output. In the current nspawn-on-shared-kernel setup, the default collection mode
is focused syscall tracepoints because live function tracing was too expensive
for batch replay on the test host.

The report fields still use the historical names `functions` and
`function_pairs`; when using `TRACE_PROFILE=syscalls`, read these as traced event
names and adjacent event pairs rather than literal kernel function coverage.

The analyzer currently reports:

- unique traced event/function names
- unique adjacent event/function pairs
- syscall-like names
- cgroup-prefixed names when function tracing is used
- VFS-related names when function tracing is used
- proc/sysfs-related names when function tracing is used
- socket/netlink-related names when function tracing is used

`analyze_queue.py` uses the initial weighted sketch from the research plan:

```text
10 * new kernel functions
+ 25 * new cgroup functions
+ 20 * new vfs functions
+ 20 * new socket/netlink functions
+ 15 * new proc/sysfs functions
+  5 * new function pairs
```

This is a low-cost approximation, not a final debloating oracle. The implemented
comparison path is now:

```text
manual baseline vs AFL queue vs AFL + trace-selected reseeded corpus
```

## Current experimental finding

A first successful end-to-end run used the fast `systemctl_generator`, semantic
preselection, real nspawn replay, and focused syscall tracepoints. The initial
8-input comparison showed a small improvement after reseeding:

```text
first-8     unique=14  pairs=119  score=735
reseeded-8  unique=14  pairs=121  score=745
```

But the 16-input comparison showed that naive top-score reseeding narrowed the
corpus instead of broadening it:

```text
first-16     unique=18  pairs=138  score=870
reseeded-16  unique=14  pairs=125  score=765
```

Interpretation: trace-aware reseeding is viable as a workflow, but selecting only
the highest-scoring inputs can overfit to one workload family. The next selection
policy should preserve diversity across semantic operation families, not just
maximize immediate novelty score.

## TODO

- [ ] Rename report fields or add a `trace_mode` column so syscall-tracepoint
      runs do not overclaim `unique_functions`.
- [ ] Change `select_corpus.py` from pure top-K scoring to diversity-preserving
      selection: bucket by semantic family signature, take the best seed from
      each bucket, then fill remaining slots by score.
- [ ] Add a compact comparison helper for the successful manual commands used in
      the run: first/reseeded, 8-input/16-input, final unique events, pairs, and
      aggregate score.
- [ ] Add a second replay profile for slower administrative commands
      (`daemon-reload`, `enable`, `disable`, `mask`, `unmask`) so they can be
      evaluated separately from the `fast-safe` profile.
- [ ] Keep syscall tracepoints as the default batch replay signal, and reserve
      `TRACE_PROFILE=focused` function tracing for tiny hand-picked second-pass
      corpora.
- [ ] Repeat the 8-input and 16-input comparisons across multiple AFL seeds or
      durations to separate real effects from run-to-run variance.
- [ ] Extend the same generation/replay/selection workflow to `unit_file_harness`
      and compare command-sequence fuzzing against generated unit-file fuzzing.
- [ ] Implement the next workload families: transient units via `systemd-run` and
      safe cgroup-property fuzzing against `fuzz-sleep.service`.

## Suggested next extensions

The next high-value workload families remain:

1. transient units via `systemd-run`
2. safe cgroup-property fuzzing against the controlled sleep service

Those should probe cgroups, namespaces, and lifecycle cleanup more directly than
adding more plain command permutations.


## Why nspawn is acceptable here

`systemd-nspawn` is not kernel isolation. It is a practical isolation boundary for
this PoC because it gives you:

- a disposable root filesystem
- a real `systemd` instance for controlled unit testing
- repeatable fake-service state without touching host services

The tradeoff is that ftrace observes the shared outer-VM kernel, so traces are
noisier than they would be in a dedicated VM. For this stage, that is acceptable
if you keep workloads short, reset state between inputs, and compare experiments
under the same tracing filters and host conditions.
