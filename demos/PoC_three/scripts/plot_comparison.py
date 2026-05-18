#!/usr/bin/env python3
from __future__ import annotations

import argparse
import csv
from pathlib import Path


def read_series(path: Path) -> list[int]:
    return [int(row["total_seen_functions"]) for row in csv.DictReader(path.open())]


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--command-afl", type=Path, required=True)
    parser.add_argument("--command-reseed", type=Path, required=True)
    parser.add_argument("--unit-afl", type=Path, required=True)
    parser.add_argument("--unit-reseed", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    try:
        import matplotlib.pyplot as plt
    except ImportError as exc:
        raise SystemExit("matplotlib is required for plotting") from exc

    series = [
        ("command AFL", read_series(args.command_afl)),
        ("command reseeded", read_series(args.command_reseed)),
        ("unit-file AFL", read_series(args.unit_afl)),
        ("unit-file reseeded", read_series(args.unit_reseed)),
    ]
    for label, values in series:
        plt.plot(range(1, len(values) + 1), values, label=label)
    plt.xlabel("queue inputs replayed")
    plt.ylabel("cumulative unique functions")
    plt.legend()
    plt.tight_layout()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    plt.savefig(args.output)


if __name__ == "__main__":
    main()
