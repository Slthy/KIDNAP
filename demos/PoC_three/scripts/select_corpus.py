#!/usr/bin/env python3
from __future__ import annotations

import argparse
import csv
import hashlib
import shutil
from collections import defaultdict
from pathlib import Path

SAFE_COMMAND_COUNT = 27
MAX_OPS = 64
OP_FAMILIES = [
    "meta", "lifecycle", "lifecycle", "lifecycle",
    "lifecycle", "query", "query", "query",
    "meta", "query", "query", "query",
    "meta", "meta", "meta", "meta",
    "failure", "fs", "lifecycle", "lifecycle",
    "lifecycle", "timer", "timer", "socket",
    "socket", "path", "path",
]


def resolve_queue(path: Path) -> Path:
    if (path / "default" / "queue").is_dir():
        return path / "default" / "queue"
    if (path / "queue").is_dir():
        return path / "queue"
    return path


def family_signature(path: Path) -> str:
    data = path.read_bytes()
    families: list[str] = []
    complete_len = min(len(data) - len(data) % 4, MAX_OPS * 4)
    for offset in range(0, complete_len, 4):
        op = data[offset] % SAFE_COMMAND_COUNT
        family = OP_FAMILIES[op]
        if not families or families[-1] != family:
            families.append(family)
    if not families:
        return "empty"
    return "-".join(families[:8])


def row_score(row: dict[str, str]) -> tuple[int, int, int]:
    new_events = int(row.get("new_events") or row.get("new_functions") or 0)
    new_pairs = int(row.get("new_event_pairs") or row.get("new_function_pairs") or 0)
    return (int(row["score"]), new_events, new_pairs)


def choose_diverse(
    rows: list[dict[str, str]], queue: Path, top_k: int, min_score: int
) -> list[dict[str, str]]:
    eligible = [row for row in rows if int(row["score"]) >= min_score]
    buckets: dict[str, list[dict[str, str]]] = defaultdict(list)
    for row in eligible:
        row["_family_signature"] = family_signature(queue / row["input"])
        buckets[row["_family_signature"]].append(row)

    chosen: list[dict[str, str]] = []
    chosen_inputs: set[str] = set()
    bucket_order = sorted(
        buckets,
        key=lambda signature: row_score(max(buckets[signature], key=row_score)),
        reverse=True,
    )
    for signature in bucket_order:
        best = max(buckets[signature], key=row_score)
        chosen.append(best)
        chosen_inputs.add(best["input"])
        if len(chosen) >= top_k:
            return chosen

    remaining = sorted(
        (row for row in eligible if row["input"] not in chosen_inputs),
        key=row_score,
        reverse=True,
    )
    chosen.extend(remaining[: top_k - len(chosen)])
    return chosen


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("csv_file", type=Path)
    parser.add_argument("queue", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--top-k", type=int, default=20)
    parser.add_argument("--min-score", type=int, default=1)
    parser.add_argument("--strategy", choices=("diverse", "top-score"), default="diverse")
    args = parser.parse_args()

    queue = resolve_queue(args.queue)
    rows = list(csv.DictReader(args.csv_file.open()))
    if args.strategy == "top-score":
        chosen = sorted(
            (row for row in rows if int(row["score"]) >= args.min_score),
            key=row_score,
            reverse=True,
        )[: args.top_k]
    else:
        chosen = choose_diverse(rows, queue, args.top_k, args.min_score)

    args.output.mkdir(parents=True, exist_ok=True)
    for idx, row in enumerate(chosen):
        src = queue / row["input"]
        signature = row.get("_family_signature", "top-score")[:80]
        digest = hashlib.sha1(src.read_bytes()).hexdigest()[:12]
        dst = args.output / f"score-{int(row['score']):06d}-{idx:03d}-{signature}-{digest}"
        shutil.copy2(src, dst)

    print(f"selected {len(chosen)} inputs into {args.output} with {args.strategy}")


if __name__ == "__main__":
    main()
