#!/usr/bin/env python3

import argparse
import json
import os
import subprocess
import tempfile
from pathlib import Path

DEFAULT_TARGET = Path(__file__).resolve().parents[1] / "configlet_sim"
DEFAULT_GROUND_TRUTH = Path(__file__).resolve().parents[1] / "ground_truth.json"


def load_ground_truth(path):
    if path is None or not path.exists():
        return None

    with open(path, "r", encoding="utf-8") as f:
        data = json.load(f)

    return {
        "features": set(data.get("features", [])),
        "combos": set(data.get("combos", [])),
        "dependencies": set(data.get("dependencies", data.get("deps", []))),
    }


def resolve_queue_dir(path: Path) -> Path:
    if path.name == "queue" and path.is_dir():
        return path

    default_queue = path / "default" / "queue"
    if default_queue.is_dir():
        return default_queue

    queue = path / "queue"
    if queue.is_dir():
        return queue

    return path


def count_line(label, discovered, ground_truth):
    display_label = f"{label}:"
    if ground_truth is None:
        return f"  {display_label:<14} {len(discovered)} / ?"

    known = ground_truth[label]
    return f"  {display_label:<14} {len(discovered & known)} / {len(known)}"


def run_input(target: Path, path: Path):
    with tempfile.NamedTemporaryFile(delete=False) as tmp:
        feedback_path = tmp.name

    env = os.environ.copy()
    env["CONFIGLET_FEEDBACK_FILE"] = feedback_path

    subprocess.run(
        [str(target), str(path)],
        env=env,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        timeout=1,
        check=False,
    )

    features = set()
    combos = set()
    deps = set()

    with open(feedback_path, "r", errors="ignore") as f:
        for line in f:
            line = line.strip()

            if line.startswith("FEATURE:"):
                features.add(line[len("FEATURE:"):])

            elif line.startswith("COMBO:"):
                combos.add(line[len("COMBO:"):])

            elif line.startswith("DEP:"):
                deps.add(line[len("DEP:"):])

    os.unlink(feedback_path)
    return features, combos, deps


def analyze(queue_dir: Path, target: Path, ground_truth):
    queue_dir = resolve_queue_dir(queue_dir)
    all_features = set()
    all_combos = set()
    all_deps = set()

    files = sorted(p for p in queue_dir.iterdir() if p.is_file())

    for p in files:
        try:
            features, combos, deps = run_input(target, p)
        except Exception:
            continue

        all_features |= features
        all_combos |= combos
        all_deps |= deps

    discovered = {
        "features": all_features,
        "combos": all_combos,
        "dependencies": all_deps,
    }
    covered = {}
    totals = {}
    for label, values in discovered.items():
        if ground_truth is None:
            covered[label] = len(values)
            totals[label] = None
        else:
            covered[label] = len(values & ground_truth[label])
            totals[label] = len(ground_truth[label])

    covered_total = sum(covered.values())
    discovered_total = sum(len(values) for values in discovered.values())
    total = None if ground_truth is None else sum(totals.values())
    percent = None if not total else covered_total / total * 100

    return {
        "queue_dir": str(queue_dir),
        "target": str(target),
        "files": len(files),
        "unique_features": len(all_features),
        "unique_combinations": len(all_combos),
        "unique_dependencies": len(all_deps),
        "covered": covered,
        "totals": totals,
        "covered_total": covered_total,
        "discovered_total": discovered_total,
        "ground_truth_total": total,
        "percent": percent,
        "features": sorted(all_features),
        "combos": sorted(all_combos),
        "dependencies": sorted(all_deps),
    }


def print_text(result, ground_truth):
    print("Configlet simulator queue analysis")
    print(f"  queue:                 {result['queue_dir']}")
    print(f"  target:                {result['target']}")
    print(f"  files:                 {result['files']}")
    print(f"  unique features:       {result['unique_features']}")
    print(f"  unique combinations:   {result['unique_combinations']}")
    print(f"  unique dependencies:   {result['unique_dependencies']}")
    print()
    print("Ground-truth comparison:")
    print(count_line("features", set(result["features"]), ground_truth))
    print(count_line("combos", set(result["combos"]), ground_truth))
    print(count_line("dependencies", set(result["dependencies"]), ground_truth))
    if ground_truth is None:
        print(f"  {'total:':<14} {result['discovered_total']} / ?")
    else:
        print(
            f"  {'total:':<14} {result['covered_total']} / "
            f"{result['ground_truth_total']} ({result['percent']:.1f}%)"
        )
    print()
    print("Features:")
    for x in result["features"]:
        print(f"  {x}")

    print()
    print("Combinations:")
    for x in result["combos"]:
        print(f"  {x}")

    print()
    print("Dependencies:")
    for x in result["dependencies"]:
        print(f"  {x}")


def main():
    parser = argparse.ArgumentParser(description="Analyze PoC_two AFL queue semantic coverage.")
    parser.add_argument("queue_dir", type=Path, help="AFL output directory or queue directory to replay")
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
        help="ground-truth JSON for discovered/total comparison (default: %(default)s)",
    )
    parser.add_argument(
        "--format",
        choices=("text", "json"),
        default="text",
        help="report format (default: text)",
    )
    args = parser.parse_args()

    ground_truth = load_ground_truth(args.ground_truth)
    result = analyze(args.queue_dir, args.target, ground_truth)

    if args.format == "json":
        print(json.dumps(result, indent=2, sort_keys=True))
    else:
        print_text(result, ground_truth)


if __name__ == "__main__":
    main()
