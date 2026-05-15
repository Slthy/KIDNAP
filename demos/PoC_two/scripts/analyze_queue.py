#!/usr/bin/env python3

import os, sys, subprocess, tempfile
from pathlib import Path

TARGET = "./configlet_sim"

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
    if len(sys.argv) != 2:
        print(f"usage: {sys.argv[0]} <afl_queue_dir>")
        sys.exit(1)

    queue_dir = Path(sys.argv[1])

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