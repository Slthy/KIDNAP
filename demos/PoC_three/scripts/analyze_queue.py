#!/usr/bin/env python3
from __future__ import annotations

import argparse
import csv
import json
import subprocess
from pathlib import Path

from analyze_trace import parse_trace, summarize


def queue_files(path: Path) -> list[Path]:
    if (path / "default" / "queue").is_dir():
        path = path / "default" / "queue"
    elif (path / "queue").is_dir():
        path = path / "queue"
    return sorted(p for p in path.iterdir() if p.is_file() and not p.name.startswith("."))


def score(
    summary: dict[str, object], seen_funcs: set[str], seen_pairs: set[tuple[str, str]]
) -> tuple[int, set[str], set[tuple[str, str]]]:
    funcs = set(summary["functions"])
    new_funcs = funcs - seen_funcs
    subsystem_funcs = summary["subsystem_functions"]
    pairs = {tuple(pair) for pair in summary["function_pairs"]}
    new_pairs = pairs - seen_pairs
    return (
        10 * len(new_funcs)
        + 25 * len(set(subsystem_funcs["cgroup"]) - seen_funcs)
        + 20 * len(set(subsystem_funcs["vfs"]) - seen_funcs)
        + 20 * len(set(subsystem_funcs["socket_netlink"]) - seen_funcs)
        + 15 * len(set(subsystem_funcs["proc_sysfs"]) - seen_funcs)
        + 5 * len(new_pairs),
        new_funcs,
        new_pairs,
    )


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("queue", type=Path)
    parser.add_argument("--harness", default="./systemctl_harness")
    parser.add_argument("--collector", default="./scripts/collect_ftrace.sh")
    parser.add_argument("--reset-cmd")
    parser.add_argument("--trace-dir", type=Path, default=Path("results/queue-traces"))
    parser.add_argument("--csv", type=Path)
    args = parser.parse_args()

    args.trace_dir.mkdir(parents=True, exist_ok=True)
    seen_funcs: set[str] = set()
    seen_pairs: set[tuple[str, str]] = set()
    seen_subsystems = {
        "cgroup": set(),
        "vfs": set(),
        "proc_sysfs": set(),
        "socket_netlink": set(),
    }
    rows: list[dict[str, object]] = []

    for testcase in queue_files(args.queue):
        if args.reset_cmd:
            subprocess.run([args.reset_cmd], check=False)
        trace = args.trace_dir / f"{testcase.name}.trace"
        subprocess.run(
            [args.collector, str(trace), "--", args.harness, str(testcase)],
            check=False,
        )
        summary = summarize(parse_trace(trace))
        novelty_score, new_funcs, new_pairs = score(summary, seen_funcs, seen_pairs)
        seen_funcs.update(summary["functions"])
        seen_pairs.update(tuple(pair) for pair in summary["function_pairs"])
        for name, funcs in summary["subsystem_functions"].items():
            seen_subsystems[name].update(funcs)
        rows.append(
            {
                "input": testcase.name,
                "score": novelty_score,
                "new_functions": len(new_funcs),
                "new_function_pairs": len(new_pairs),
                "total_seen_functions": len(seen_funcs),
                "total_seen_function_pairs": len(seen_pairs),
                "trace_cgroup": summary["subsystems"]["cgroup"],
                "trace_vfs": summary["subsystems"]["vfs"],
                "trace_proc_sysfs": summary["subsystems"]["proc_sysfs"],
                "trace_socket_netlink": summary["subsystems"]["socket_netlink"],
                "total_seen_cgroup": len(seen_subsystems["cgroup"]),
                "total_seen_vfs": len(seen_subsystems["vfs"]),
                "total_seen_proc_sysfs": len(seen_subsystems["proc_sysfs"]),
                "total_seen_socket_netlink": len(seen_subsystems["socket_netlink"]),
            }
        )

    print(json.dumps(rows, indent=2))
    if args.csv:
        with args.csv.open("w", newline="") as fp:
            writer = csv.DictWriter(fp, fieldnames=rows[0].keys() if rows else ["input"])
            writer.writeheader()
            writer.writerows(rows)


if __name__ == "__main__":
    main()
