#!/usr/bin/env python3
from __future__ import annotations

import argparse
import hashlib
import shutil
from pathlib import Path

OP_MAX = 27
WIDTH = 4
MAX_OPS = 64
MAX_REPEAT = 3
# Exclude metadata/admin operations from the default first-pass replay profile.
# Keep lifecycle/query/failure/fs/timer/socket/path operations.
FAST_SAFE_OPS = {1, 2, 3, 4, 5, 6, 7, 10, 11, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26}


def queue_files(path: Path) -> list[Path]:
    if (path / 'default' / 'queue').is_dir():
        path = path / 'default' / 'queue'
    elif (path / 'queue').is_dir():
        path = path / 'queue'
    return sorted(p for p in path.iterdir() if p.is_file() and not p.name.startswith('.'))


def semantic_shape(data: bytes) -> tuple:
    ops = []
    for i in range(0, min(len(data) // WIDTH, MAX_OPS) * WIDTH, WIDTH):
        op = data[i] % OP_MAX
        repeat = (data[i + 3] % MAX_REPEAT) + 1
        ops.append((op, repeat))
    families = tuple(op // 4 for op, _ in ops)
    pairs = tuple(zip((op for op, _ in ops), (op for op, _ in ops[1:])))
    return (len(ops), tuple(ops), families, pairs)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument('queue', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--limit', type=int, default=64)
    parser.add_argument('--max-ops', type=int, default=12)
    parser.add_argument('--max-expanded-commands', type=int, default=24)
    parser.add_argument('--profile', choices=('fast-safe', 'all'), default='fast-safe')
    args = parser.parse_args()

    by_shape: dict[tuple, tuple[Path, int]] = {}
    for testcase in queue_files(args.queue):
        data = testcase.read_bytes()
        shape = semantic_shape(data)
        expanded_commands = sum(repeat for _, repeat in shape[1])
        if shape[0] > args.max_ops or expanded_commands > args.max_expanded_commands:
            continue
        if args.profile == 'fast-safe' and any(op not in FAST_SAFE_OPS for op, _ in shape[1]):
            continue
        # Prefer shorter representatives for the same semantic shape.
        current = by_shape.get(shape)
        candidate = (testcase, len(data))
        if current is None or candidate[1] < current[1]:
            by_shape[shape] = candidate

    ranked = sorted(
        by_shape.values(),
        key=lambda item: (
            len(set(semantic_shape(item[0].read_bytes())[2])),
            len(set(semantic_shape(item[0].read_bytes())[3])),
            len(semantic_shape(item[0].read_bytes())[1]),
            -item[1],
        ),
        reverse=True,
    )[: args.limit]

    if args.output.exists():
        shutil.rmtree(args.output)
    args.output.mkdir(parents=True, exist_ok=True)
    for idx, (src, _) in enumerate(ranked):
        digest = hashlib.sha1(src.read_bytes()).hexdigest()[:8]
        shutil.copy2(src, args.output / f'pre-{idx:03d}-{digest}-{src.name}')

    print(f'preselected {len(ranked)} of {len(by_shape)} semantic shapes into {args.output}')


if __name__ == '__main__':
    main()
