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
    "unique_functions",
    "unique_function_pairs",
    "cgroup",
    "vfs",
    "proc_sysfs",
    "socket_netlink",
    "queue_inputs",
    "final_score",
]


def baseline_row(path: Path) -> dict[str, object]:
    data = json.loads(path.read_text())
    subsystems = data.get("subsystems", {})
    return {
        "experiment": "manual-baseline",
        "family": "command-sequence",
        "mode": "manual",
        "unique_functions": data.get("unique_functions", 0),
        "unique_function_pairs": data.get("unique_function_pairs", 0),
        "cgroup": subsystems.get("cgroup", 0),
        "vfs": subsystems.get("vfs", 0),
        "proc_sysfs": subsystems.get("proc_sysfs", 0),
        "socket_netlink": subsystems.get("socket_netlink", 0),
        "queue_inputs": 1,
        "final_score": "",
    }


def queue_row(path: Path, experiment: str, family: str, mode: str) -> dict[str, object]:
    rows = list(csv.DictReader(path.open()))
    if not rows:
        raise ValueError(f"empty queue CSV: {path}")
    last = rows[-1]
    return {
        "experiment": experiment,
        "family": family,
        "mode": mode,
        "unique_functions": int(last["total_seen_functions"]),
        "unique_function_pairs": int(last["total_seen_function_pairs"]),
        "cgroup": int(last["total_seen_cgroup"]),
        "vfs": int(last["total_seen_vfs"]),
        "proc_sysfs": int(last["total_seen_proc_sysfs"]),
        "socket_netlink": int(last["total_seen_socket_netlink"]),
        "queue_inputs": len(rows),
        "final_score": sum(int(row["score"]) for row in rows),
    }


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--baseline-json", type=Path, required=True)
    parser.add_argument("--command-afl", type=Path, required=True)
    parser.add_argument("--command-reseed", type=Path, required=True)
    parser.add_argument("--unit-afl", type=Path, required=True)
    parser.add_argument("--unit-reseed", type=Path, required=True)
    parser.add_argument("--csv", type=Path, required=True)
    parser.add_argument("--json", type=Path)
    args = parser.parse_args()

    rows = [
        baseline_row(args.baseline_json),
        queue_row(args.command_afl, "command-afl", "command-sequence", "afl"),
        queue_row(args.command_reseed, "command-reseed", "command-sequence", "reseeded"),
        queue_row(args.unit_afl, "unitfile-afl", "unit-file", "afl"),
        queue_row(args.unit_reseed, "unitfile-reseed", "unit-file", "reseeded"),
    ]

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
