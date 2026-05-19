#!/usr/bin/env python3
from __future__ import annotations

import argparse
import csv
import json
from pathlib import Path

FIELDS = [
    "experiment",
    "family",
    "mode",
    "selection",
    "source",
    "trace_mode",
    "unique_events",
    "unique_event_pairs",
    "cgroup",
    "vfs",
    "proc_sysfs",
    "socket_netlink",
    "queue_inputs",
    "final_score",
]


def score_key(row: dict[str, object]) -> tuple[int, int, int]:
    return (
        int(row["unique_events"]),
        int(row["unique_event_pairs"]),
        int(row["final_score"] or 0),
    )


def baseline_row(path: Path) -> dict[str, object]:
    data = json.loads(path.read_text())
    subsystems = data.get("subsystems", {})
    return {
        "experiment": "manual-baseline",
        "family": "command-sequence",
        "mode": "manual",
        "selection": "single",
        "source": path.parent.name,
        "trace_mode": data.get("trace_mode", "unknown"),
        "unique_events": data.get("unique_events", data.get("unique_functions", 0)),
        "unique_event_pairs": data.get("unique_event_pairs", data.get("unique_function_pairs", 0)),
        "cgroup": subsystems.get("cgroup", 0),
        "vfs": subsystems.get("vfs", 0),
        "proc_sysfs": subsystems.get("proc_sysfs", 0),
        "socket_netlink": subsystems.get("socket_netlink", 0),
        "queue_inputs": 1,
        "final_score": "",
    }


def queue_row(path: Path, experiment: str, family: str, mode: str, selection: str) -> dict[str, object]:
    rows = list(csv.DictReader(path.open()))
    if not rows:
        raise ValueError(f"empty queue CSV: {path}")
    last = rows[-1]
    return {
        "experiment": experiment,
        "family": family,
        "mode": mode,
        "selection": selection,
        "source": path.parent.name if path.parent.name.startswith("round-") else str(path.parent),
        "trace_mode": last.get("trace_mode", "unknown"),
        "unique_events": int(last.get("total_seen_events") or last["total_seen_functions"]),
        "unique_event_pairs": int(last.get("total_seen_event_pairs") or last["total_seen_function_pairs"]),
        "cgroup": int(last["total_seen_cgroup"]),
        "vfs": int(last["total_seen_vfs"]),
        "proc_sysfs": int(last["total_seen_proc_sysfs"]),
        "socket_netlink": int(last["total_seen_socket_netlink"]),
        "queue_inputs": len(rows),
        "final_score": sum(int(row["score"]) for row in rows),
    }


def best_round_row(paths: list[Path], experiment: str, family: str, mode: str) -> dict[str, object] | None:
    if not paths:
        return None
    rows = [queue_row(path, experiment, family, mode, "best") for path in paths]
    return max(rows, key=score_key)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--baseline-json", type=Path, required=True)
    parser.add_argument("--command-afl", type=Path, required=True)
    parser.add_argument("--command-reseed", type=Path, required=True)
    parser.add_argument("--command-reseed-round", type=Path, action="append", default=[])
    parser.add_argument("--unit-afl", type=Path, required=True)
    parser.add_argument("--unit-reseed", type=Path, required=True)
    parser.add_argument("--unit-reseed-round", type=Path, action="append", default=[])
    parser.add_argument("--csv", type=Path, required=True)
    parser.add_argument("--json", type=Path)
    args = parser.parse_args()

    rows = [
        baseline_row(args.baseline_json),
        queue_row(args.command_afl, "command-afl", "command-sequence", "afl", "single"),
        queue_row(args.command_reseed, "command-reseed", "command-sequence", "reseeded", "final"),
    ]
    command_best = best_round_row(
        args.command_reseed_round, "command-reseed", "command-sequence", "reseeded"
    )
    if command_best:
        rows.append(command_best)
    rows.extend([
        queue_row(args.unit_afl, "unitfile-afl", "unit-file", "afl", "single"),
        queue_row(args.unit_reseed, "unitfile-reseed", "unit-file", "reseeded", "final"),
    ])
    unit_best = best_round_row(
        args.unit_reseed_round, "unitfile-reseed", "unit-file", "reseeded"
    )
    if unit_best:
        rows.append(unit_best)

    args.csv.parent.mkdir(parents=True, exist_ok=True)
    with args.csv.open("w", newline="") as fp:
        writer = csv.DictWriter(fp, fieldnames=FIELDS)
        writer.writeheader()
        writer.writerows(rows)
    if args.json:
        args.json.write_text(json.dumps(rows, indent=2))
    print(json.dumps(rows, indent=2))


if __name__ == "__main__":
    main()
