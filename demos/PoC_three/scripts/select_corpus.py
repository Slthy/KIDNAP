#!/usr/bin/env python3
from __future__ import annotations

import argparse
import csv
import shutil
from pathlib import Path


def resolve_queue(path: Path) -> Path:
    if (path / "default" / "queue").is_dir():
        return path / "default" / "queue"
    if (path / "queue").is_dir():
        return path / "queue"
    return path


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("csv_file", type=Path)
    parser.add_argument("queue", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--top-k", type=int, default=20)
    parser.add_argument("--min-score", type=int, default=1)
    args = parser.parse_args()

    queue = resolve_queue(args.queue)
    rows = list(csv.DictReader(args.csv_file.open()))
    chosen = sorted(
        (row for row in rows if int(row["score"]) >= args.min_score),
        key=lambda row: (int(row["score"]), int(row["new_functions"])),
        reverse=True,
    )[: args.top_k]

    args.output.mkdir(parents=True, exist_ok=True)
    for idx, row in enumerate(chosen):
        src = queue / row["input"]
        dst = args.output / f"score-{int(row['score']):06d}-{idx:03d}-{src.name}"
        shutil.copy2(src, dst)

    print(f"selected {len(chosen)} inputs into {args.output}")


if __name__ == "__main__":
    main()
