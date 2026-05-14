#!/usr/bin/env python3
"""Plot or export PoC_one AFL queue growth metrics.

The script derives syscall-feedback-style metrics directly from queued testcase
bytes, so it works for both baseline and SYSCALL_FEEDBACK fuzzing output dirs.
If matplotlib is unavailable, use --csv to export the same time-series data.
"""

from __future__ import annotations

import argparse
import csv
import sys
from collections.abc import Sequence
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

from analyze_queue import afl_queue_files, decode_bytes  # noqa: E402


def label_for_path(path: Path) -> str:
    """Return a stable label for AFL output, instance, or queue paths."""
    if path.name == "queue" and path.parent.name:
        return f"{path.parent.parent.name}/{path.parent.name}"
    if path.name == "default" and path.parent.name:
        return f"{path.parent.name}/{path.name}"
    return path.name


def series_for_path(path: Path) -> list[dict[str, object]]:
    files = sorted(
        afl_queue_files([path]), key=lambda item: (item.stat().st_mtime, item.name)
    )
    seen_syscalls: set[str] = set()
    seen_seq2: set[str] = set()
    seen_seq3: set[str] = set()
    rows: list[dict[str, object]] = []
    start_time = files[0].stat().st_mtime if files else 0.0
    label = label_for_path(path)

    for index, file_path in enumerate(files, start=1):
        ops = decode_bytes(file_path.read_bytes())
        seen_syscalls.update(op.syscall for op in ops)
        seen_seq2.update(
            f"{first.syscall}->{second.syscall}" for first, second in zip(ops, ops[1:])
        )
        seen_seq3.update(
            f"{first.syscall}->{second.syscall}->{third.syscall}"
            for first, second, third in zip(ops, ops[1:], ops[2:])
        )
        rows.append(
            {
                "label": label,
                "file_index": index,
                "seconds": file_path.stat().st_mtime - start_time,
                "queued_files": index,
                "unique_syscalls": len(seen_syscalls),
                "unique_seq2": len(seen_seq2),
                "unique_seq3": len(seen_seq3),
            }
        )
    return rows


def write_csv(path: Path, rows: Sequence[dict[str, object]]) -> None:
    with path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(
            handle,
            fieldnames=[
                "label",
                "file_index",
                "seconds",
                "queued_files",
                "unique_syscalls",
                "unique_seq2",
                "unique_seq3",
            ],
        )
        writer.writeheader()
        writer.writerows(rows)


def saturation_seconds(rows: Sequence[dict[str, object]], metric: str) -> float:
    """Return when a metric first reaches its final value for this queue."""
    if not rows:
        return 0.0
    final_value = int(rows[-1][metric])
    for row in rows:
        if int(row[metric]) == final_value:
            return float(row["seconds"])
    return float(rows[-1]["seconds"])


def render_plot(path: Path, rows_by_input: dict[Path, list[dict[str, object]]]) -> None:
    try:
        import matplotlib.pyplot as plt
    except ImportError as exc:  # pragma: no cover - depends on user environment
        raise RuntimeError("matplotlib is required for plotting; rerun with --csv") from exc

    fig, axes = plt.subplots(3, 1, figsize=(10, 10), sharex=True)
    for input_path, rows in rows_by_input.items():
        if not rows:
            continue
        x_values = [float(row["seconds"]) for row in rows]
        axes[0].plot(
            x_values,
            [int(row["unique_syscalls"]) for row in rows],
            marker="o",
            label=input_path.name,
        )
        axes[1].plot(
            x_values,
            [int(row["unique_seq2"]) for row in rows],
            marker="o",
            label=input_path.name,
        )
        axes[2].plot(
            x_values,
            [int(row["unique_seq3"]) for row in rows],
            marker="o",
            label=input_path.name,
        )

    axes[0].set_ylabel("unique syscalls")
    axes[1].set_ylabel("unique seq2 transitions")
    axes[2].set_ylabel("unique seq3 transitions")
    axes[2].set_xlabel("seconds since first queue entry")
    for axis in axes:
        axis.grid(True, alpha=0.3)
        axis.legend(loc="best")
    fig.tight_layout()
    fig.savefig(path)


def parse_args(argv: Sequence[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Plot cumulative PoC_one syscall and sequence coverage from AFL queues."
    )
    parser.add_argument(
        "paths",
        nargs="+",
        type=Path,
        help="AFL output directories, queue directories, or testcase directories to compare.",
    )
    parser.add_argument(
        "--output",
        "-o",
        type=Path,
        default=Path("feedback_growth.png"),
        help="PNG output path when plotting (default: feedback_growth.png).",
    )
    parser.add_argument("--csv", type=Path, help="Write time-series data to CSV.")
    parser.add_argument(
        "--no-plot",
        action="store_true",
        help="Only write CSV/summary; do not require matplotlib or create a PNG.",
    )
    return parser.parse_args(argv)


def main(argv: Sequence[str] | None = None) -> int:
    args = parse_args(sys.argv[1:] if argv is None else argv)

    try:
        rows_by_input = {path: series_for_path(path) for path in args.paths}
    except FileNotFoundError as exc:
        print(f"error: path does not exist: {exc}", file=sys.stderr)
        return 2

    all_rows = [row for rows in rows_by_input.values() for row in rows]
    if args.csv is not None:
        write_csv(args.csv, all_rows)

    if not args.no_plot:
        try:
            render_plot(args.output, rows_by_input)
            print(f"wrote {args.output}")
        except RuntimeError as exc:
            print(f"error: {exc}", file=sys.stderr)
            return 2

    for path, rows in rows_by_input.items():
        if rows:
            last = rows[-1]
            print(
                f"{path}: files={last['queued_files']} "
                f"unique_syscalls={last['unique_syscalls']} "
                f"unique_seq2={last['unique_seq2']} "
                f"unique_seq3={last['unique_seq3']} "
                f"sat_syscalls={saturation_seconds(rows, 'unique_syscalls'):.3f}s "
                f"sat_seq2={saturation_seconds(rows, 'unique_seq2'):.3f}s "
                f"sat_seq3={saturation_seconds(rows, 'unique_seq3'):.3f}s"
            )
        else:
            print(f"{path}: no queue files")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
