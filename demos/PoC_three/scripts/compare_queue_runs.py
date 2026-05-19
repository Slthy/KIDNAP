#!/usr/bin/env python3
from __future__ import annotations

import argparse
import csv
import json
from pathlib import Path


def rows_from(path: Path) -> list[dict[str, str]]:
    rows = list(csv.DictReader(path.open()))
    if not rows:
        raise ValueError(f"empty queue CSV: {path}")
    return rows


def summarize(label: str, path: Path) -> dict[str, object]:
    rows = rows_from(path)
    row = rows[-1]
    return {
        "label": label,
        "path": str(path),
        "trace_mode": row.get("trace_mode", "unknown"),
        "queue_inputs": len(rows),
        "unique_events": int(row.get("total_seen_events") or row["total_seen_functions"]),
        "unique_event_pairs": int(row.get("total_seen_event_pairs") or row["total_seen_function_pairs"]),
        "aggregate_score": sum(int(item["score"]) for item in rows),
        "cgroup": int(row["total_seen_cgroup"]),
        "vfs": int(row["total_seen_vfs"]),
        "proc_sysfs": int(row["total_seen_proc_sysfs"]),
        "socket_netlink": int(row["total_seen_socket_netlink"]),
    }


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Print a compact comparison table for queue-growth CSV files."
    )
    parser.add_argument("runs", nargs="+", help="Run spec as LABEL=path/to/queue-growth.csv")
    parser.add_argument("--json", type=Path)
    args = parser.parse_args()

    rows: list[dict[str, object]] = []
    for spec in args.runs:
        if "=" not in spec:
            raise SystemExit(f"run must be LABEL=CSV: {spec}")
        label, raw_path = spec.split("=", 1)
        rows.append(summarize(label, Path(raw_path)))

    if args.json:
        args.json.write_text(json.dumps(rows, indent=2))

    headers = ["label", "trace", "inputs", "events", "pairs", "score", "cgroup", "vfs", "proc", "sock"]
    print(",".join(headers))
    for row in rows:
        print(",".join([
            str(row["label"]),
            str(row["trace_mode"]),
            str(row["queue_inputs"]),
            str(row["unique_events"]),
            str(row["unique_event_pairs"]),
            str(row["aggregate_score"]),
            str(row["cgroup"]),
            str(row["vfs"]),
            str(row["proc_sysfs"]),
            str(row["socket_netlink"]),
        ]))


if __name__ == "__main__":
    main()
