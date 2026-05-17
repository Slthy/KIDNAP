#!/usr/bin/env python3
"""Plot or export PoC_two semantic coverage growth metrics.

The script replays AFL queue entries through the configlet simulator and tracks
cumulative semantic features, combinations, and dependencies over time. It works
with baseline and CONFIGLET_FEEDBACK output directories because the metrics are
read from CONFIGLET_FEEDBACK_FILE during replay rather than AFL's coverage map.
If matplotlib is unavailable, use --csv to export the same time-series data.
"""

from __future__ import annotations

import argparse
import copy
import csv
import importlib.util
import re
import sys
from collections.abc import Sequence
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

from analyze_queue import (  # noqa: E402
    DEFAULT_GROUND_TRUTH,
    DEFAULT_TARGET,
    load_ground_truth,
    resolve_queue_dir,
    run_input,
)

AFL_TIME_RE = re.compile(r"(?:^|,)time:(?P<msec>\d+)(?:,|$)")
CSV_FIELDS = [
    "label",
    "file_index",
    "seconds",
    "queued_files",
    "unique_features",
    "unique_combinations",
    "unique_dependencies",
    "covered_total",
    "discovered_total",
    "ground_truth_total",
    "percent",
]


def afl_filename_seconds(path: Path) -> float | None:
    """Return AFL's queue timestamp in seconds, if present in the filename."""
    match = AFL_TIME_RE.search(path.name)
    if match is None:
        return None
    return int(match.group("msec")) / 1000.0


def queue_entry_seconds(path: Path, mtime_start: float) -> float:
    afl_seconds = afl_filename_seconds(path)
    if afl_seconds is not None:
        return afl_seconds
    return path.stat().st_mtime - mtime_start


def label_for_path(path: Path) -> str:
    """Return a stable label for AFL output, instance, or queue paths."""
    if path.name == "queue" and path.parent.name:
        return f"{path.parent.parent.name}/{path.parent.name}"
    if path.name == "default" and path.parent.name:
        return f"{path.parent.name}/{path.name}"
    return path.name


def queue_files(path: Path) -> list[Path]:
    queue_dir = resolve_queue_dir(path)
    return [file_path for file_path in queue_dir.iterdir() if file_path.is_file()]


def covered_counts(
    features: set[str],
    combos: set[str],
    dependencies: set[str],
    ground_truth: dict[str, set[str]] | None,
) -> tuple[int, int | None, float | None]:
    discovered_total = len(features) + len(combos) + len(dependencies)
    if ground_truth is None:
        return discovered_total, None, None

    covered_total = (
        len(features & ground_truth["features"])
        + len(combos & ground_truth["combos"])
        + len(dependencies & ground_truth["dependencies"])
    )
    ground_truth_total = sum(len(values) for values in ground_truth.values())
    percent = None if ground_truth_total == 0 else covered_total / ground_truth_total * 100
    return covered_total, ground_truth_total, percent


def series_for_path(
    path: Path,
    target: Path,
    ground_truth: dict[str, set[str]] | None,
) -> list[dict[str, object]]:
    unsorted_files = queue_files(path)
    start_time = min(
        (file_path.stat().st_mtime for file_path in unsorted_files), default=0.0
    )
    files = sorted(
        unsorted_files,
        key=lambda item: (
            queue_entry_seconds(item, start_time),
            item.stat().st_mtime,
            item.name,
        ),
    )

    seen_features: set[str] = set()
    seen_combinations: set[str] = set()
    seen_dependencies: set[str] = set()
    rows: list[dict[str, object]] = []
    label = label_for_path(path)

    for index, file_path in enumerate(files, start=1):
        try:
            features, combos, dependencies = run_input(target, file_path)
        except Exception as exc:  # pragma: no cover - depends on external target
            print(f"warning: skipped {file_path}: {exc}", file=sys.stderr)
            continue

        seen_features.update(features)
        seen_combinations.update(combos)
        seen_dependencies.update(dependencies)
        covered_total, ground_truth_total, percent = covered_counts(
            seen_features, seen_combinations, seen_dependencies, ground_truth
        )
        rows.append(
            {
                "label": label,
                "file_index": index,
                "seconds": queue_entry_seconds(file_path, start_time),
                "queued_files": index,
                "unique_features": len(seen_features),
                "unique_combinations": len(seen_combinations),
                "unique_dependencies": len(seen_dependencies),
                "covered_total": covered_total,
                "discovered_total": (
                    len(seen_features) + len(seen_combinations) + len(seen_dependencies)
                ),
                "ground_truth_total": ground_truth_total,
                "percent": "" if percent is None else f"{percent:.3f}",
            }
        )
    return rows


def metric_change_points(
    rows: Sequence[dict[str, object]], metric: str
) -> tuple[list[float], list[float]]:
    """Return chronological points where ``metric`` changes, plus the final point."""
    x_values: list[float] = []
    y_values: list[float] = []
    previous_value: float | None = None

    for row in rows:
        current_value = float(row[metric])
        if previous_value is None or current_value != previous_value:
            x_values.append(float(row["seconds"]))
            y_values.append(current_value)
            previous_value = current_value

    if rows:
        final_x = float(rows[-1]["seconds"])
        final_y = float(rows[-1][metric])
        if not x_values or x_values[-1] != final_x or y_values[-1] != final_y:
            x_values.append(final_x)
            y_values.append(final_y)

    return x_values, y_values


def plot_metric(
    axis, rows: Sequence[dict[str, object]], metric: str, label: str
) -> None:
    x_values, y_values = metric_change_points(rows, metric)
    if not x_values:
        return
    (line,) = axis.step(x_values, y_values, where="post", linewidth=1.8, label=label)
    axis.scatter(x_values[-1:], y_values[-1:], s=18, color=line.get_color(), zorder=3)


def extend_rows_to_duration(
    rows_by_input: dict[Path, list[dict[str, object]]], duration: float | None
) -> dict[Path, list[dict[str, object]]]:
    """Return rows with a final plateau point at ``duration`` seconds.

    AFL queue timestamps describe when new testcases were discovered. In a
    fixed-duration comparison, the last discovery often happens before AFL's
    ``-V`` limit, so extending the line makes clear that the campaign kept
    running and simply found no additional semantic states.
    """
    if duration is None:
        return rows_by_input
    extended: dict[Path, list[dict[str, object]]] = {}
    for path, rows in rows_by_input.items():
        copied_rows = [copy.deepcopy(row) for row in rows]
        if copied_rows and float(copied_rows[-1]["seconds"]) < duration:
            final_row = copy.deepcopy(copied_rows[-1])
            final_row["seconds"] = duration
            copied_rows.append(final_row)
        extended[path] = copied_rows
    return extended


def write_csv(path: Path, rows: Sequence[dict[str, object]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=CSV_FIELDS)
        writer.writeheader()
        writer.writerows(rows)


def saturation_seconds(rows: Sequence[dict[str, object]], metric: str) -> float:
    """Return when a metric first reaches its final value for this queue."""
    if not rows:
        return 0.0
    final_value = rows[-1][metric]
    for row in rows:
        if row[metric] == final_value:
            return float(row["seconds"])
    return float(rows[-1]["seconds"])


def render_plot(path: Path, rows_by_input: dict[Path, list[dict[str, object]]]) -> None:
    if importlib.util.find_spec("matplotlib") is None:  # pragma: no cover
        raise RuntimeError("matplotlib is required for plotting; rerun with --csv")

    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    fig, axes = plt.subplots(4, 1, figsize=(10, 12), sharex=True)
    for rows in rows_by_input.values():
        if not rows:
            continue
        label = str(rows[0]["label"])
        plot_metric(axes[0], rows, "unique_features", label)
        plot_metric(axes[1], rows, "unique_combinations", label)
        plot_metric(axes[2], rows, "unique_dependencies", label)
        plot_metric(axes[3], rows, "covered_total", label)

    axes[0].set_ylabel("unique features")
    axes[1].set_ylabel("unique combinations")
    axes[2].set_ylabel("unique dependencies")
    axes[3].set_ylabel("ground-truth covered total")
    axes[3].set_xlabel("seconds since fuzzing start (mtime-relative for non-AFL files)")
    for axis in axes:
        axis.grid(True, alpha=0.25)
        axis.margins(x=0.02, y=0.08)
        axis.legend(loc="best", framealpha=0.9)
    fig.tight_layout()
    path.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(path)
    plt.close(fig)


def parse_args(argv: Sequence[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Plot cumulative PoC_two configlet semantic coverage from AFL queues."
    )
    parser.add_argument(
        "paths",
        nargs="+",
        type=Path,
        help="AFL output directories, queue directories, or testcase directories to compare.",
    )
    parser.add_argument(
        "--target",
        type=Path,
        default=DEFAULT_TARGET,
        help="configlet target to replay queue entries with (default: %(default)s)",
    )
    parser.add_argument(
        "--ground-truth",
        type=Path,
        default=DEFAULT_GROUND_TRUTH,
        help="ground-truth JSON for covered/total comparison (default: %(default)s)",
    )
    parser.add_argument(
        "--output",
        "-o",
        type=Path,
        default=Path("configlet_feedback_growth.png"),
        help="PNG output path when plotting (default: configlet_feedback_growth.png).",
    )
    parser.add_argument("--csv", type=Path, help="Write time-series data to CSV.")
    parser.add_argument(
        "--no-plot",
        action="store_true",
        help="Only write CSV/summary; do not require matplotlib or create a PNG.",
    )
    parser.add_argument(
        "--extend-to",
        type=float,
        help=(
            "extend every time series with a flat final point at SEC seconds; "
            "use this with fixed-duration AFL -V runs so the PNG shows the "
            "campaign continued after the final queued discovery"
        ),
        metavar="SEC",
    )
    return parser.parse_args(argv)


def main(argv: Sequence[str] | None = None) -> int:
    args = parse_args(sys.argv[1:] if argv is None else argv)

    try:
        ground_truth = load_ground_truth(args.ground_truth)
        rows_by_input = {
            path: series_for_path(path, args.target, ground_truth) for path in args.paths
        }
        rows_by_input = extend_rows_to_duration(rows_by_input, args.extend_to)
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
                f"features={last['unique_features']} "
                f"combinations={last['unique_combinations']} "
                f"dependencies={last['unique_dependencies']} "
                f"covered_total={last['covered_total']}/{last['ground_truth_total'] or '?'} "
                f"percent={last['percent'] or '?'} "
                f"sat_features={saturation_seconds(rows, 'unique_features'):.3f}s "
                f"sat_combinations={saturation_seconds(rows, 'unique_combinations'):.3f}s "
                f"sat_dependencies={saturation_seconds(rows, 'unique_dependencies'):.3f}s"
            )
        else:
            print(f"{path}: no queue files")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
