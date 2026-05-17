#!/usr/bin/env python3

import argparse
import json
import os
import subprocess
import tempfile
from pathlib import Path

TARGET = "./configlet_sim"
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


def count_line(label, discovered, ground_truth):
    display_label = f"{label}:"
    if ground_truth is None:
        return f"  {display_label:<14} {len(discovered)} / ?"

    known = ground_truth[label]
    return f"  {display_label:<14} {len(discovered & known)} / {len(known)}"


def run_input(path: Path):
    with tempfile.NamedTemporaryFile(delete=False) as tmp:
        feedback_path = tmp.name

    env = os.environ.copy()
    env["CONFIGLET_FEEDBACK_FILE"] = feedback_path

    subprocess.run(
        [TARGET, str(path)],
        env=env,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        timeout=1,
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


def main():
    parser = argparse.ArgumentParser(description="Analyze PoC_two AFL queue semantic coverage.")
    parser.add_argument("queue_dir", type=Path, help="AFL queue directory to replay")
    parser.add_argument(
        "--ground-truth",
        type=Path,
        default=DEFAULT_GROUND_TRUTH,
        help="ground-truth JSON for discovered/total comparison (default: %(default)s)",
    )
    args = parser.parse_args()

    queue_dir = args.queue_dir
    ground_truth = load_ground_truth(args.ground_truth)

    all_features = set()
    all_combos = set()
    all_deps = set()

    files = sorted(p for p in queue_dir.iterdir() if p.is_file())

    for p in files:
        try:
            features, combos, deps = run_input(p)
        except Exception:
            continue

        all_features |= features
        all_combos |= combos
        all_deps |= deps

    print("Configlet simulator queue analysis")
    print(f"  files:                 {len(files)}")
    print(f"  unique features:       {len(all_features)}")
    print(f"  unique combinations:   {len(all_combos)}")
    print(f"  unique dependencies:   {len(all_deps)}")
    print()
    print("Ground-truth comparison:")
    print(count_line("features", all_features, ground_truth))
    print(count_line("combos", all_combos, ground_truth))
    print(count_line("dependencies", all_deps, ground_truth))
    discovered_total = len(all_features) + len(all_combos) + len(all_deps)
    if ground_truth is None:
        print(f"  {'total:':<14} {discovered_total} / ?")
    else:
        covered_total = (
            len(all_features & ground_truth["features"])
            + len(all_combos & ground_truth["combos"])
            + len(all_deps & ground_truth["dependencies"])
        )
        ground_truth_total = sum(len(values) for values in ground_truth.values())
        percent = (covered_total / ground_truth_total * 100) if ground_truth_total else 0
        print(f"  {'total:':<14} {covered_total} / {ground_truth_total} ({percent:.1f}%)")
    print()
    print("Features:")
    for x in sorted(all_features):
        print(f"  {x}")

    print()
    print("Combinations:")
    for x in sorted(all_combos):
        print(f"  {x}")

    print()
    print("Dependencies:")
    for x in sorted(all_deps):
        print(f"  {x}")


if __name__ == "__main__":
    main()