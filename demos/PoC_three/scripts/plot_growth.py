#!/usr/bin/env python3
from __future__ import annotations

import argparse
import csv
from pathlib import Path


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("csv_file", type=Path)
    parser.add_argument("--output", type=Path, default=Path("ftrace_growth.png"))
    args = parser.parse_args()

    rows = list(csv.DictReader(args.csv_file.open()))
    if not rows:
        raise SystemExit("CSV has no rows")
    try:
        import matplotlib.pyplot as plt
    except ImportError as exc:
        raise SystemExit("matplotlib is required for plotting") from exc

    x = list(range(1, len(rows) + 1))
    plt.plot(x, [int(r.get("total_seen_events") or r["total_seen_functions"]) for r in rows], label="unique events")
    plt.xlabel("queue inputs replayed")
    plt.ylabel("cumulative unique events")
    plt.legend()
    plt.tight_layout()
    plt.savefig(args.output)


if __name__ == "__main__":
    main()
