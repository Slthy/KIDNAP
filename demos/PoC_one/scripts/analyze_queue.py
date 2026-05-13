#!/usr/bin/env python3
"""Analyze PoC_one AFL queues or standalone testcase files.

The PoC_one target consumes 4-byte operations: syscall_id, arg0, arg1, arg2.
This helper decodes those operations and summarizes the syscall-level feedback
signals that the instrumented target is expected to expose to AFL.
"""

from __future__ import annotations

import argparse
import csv
import json
import subprocess
import sys
from collections import Counter
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Iterable, Iterator, Sequence

OP_SIZE = 4
MAX_OPS = 64
SYSCALLS = [
    "openat",
    "read",
    "write",
    "close",
    "fstat",
    "mmap",
    "mprotect",
    "socket",
    "bind",
    "setsockopt",
    "getpid",
    "uname",
]


@dataclass(frozen=True)
class Operation:
    index: int
    offset: int
    syscall_id: int
    arg0: int
    arg1: int
    arg2: int
    syscall: str


@dataclass
class CaseSummary:
    path: str
    size: int
    op_count: int
    trailing_bytes: int
    syscalls: dict[str, int]
    seq2: dict[str, int]
    exit_code: int | None = None
    timed_out: bool | None = None


def afl_queue_files(paths: Sequence[Path]) -> Iterator[Path]:
    """Yield AFL queue files and direct testcase files from the provided paths."""
    for root in paths:
        if root.is_file():
            yield root
            continue

        if not root.exists():
            raise FileNotFoundError(root)

        queue = root / "queue"
        search_root = queue if queue.is_dir() else root
        for item in sorted(search_root.iterdir()):
            if item.is_file() and not item.name.startswith("."):
                yield item


def decode_bytes(data: bytes, max_ops: int = MAX_OPS) -> list[Operation]:
    usable = min(len(data), max_ops * OP_SIZE)
    operations: list[Operation] = []
    for index, offset in enumerate(range(0, usable - (usable % OP_SIZE), OP_SIZE)):
        syscall_id, arg0, arg1, arg2 = data[offset : offset + OP_SIZE]
        syscall = SYSCALLS[syscall_id % len(SYSCALLS)]
        operations.append(
            Operation(index, offset, syscall_id, arg0, arg1, arg2, syscall)
        )
    return operations


def summarize_case(path: Path, target: Path | None, timeout: float) -> CaseSummary:
    data = path.read_bytes()
    ops = decode_bytes(data)
    syscall_counts = Counter(op.syscall for op in ops)
    seq_counts = Counter(
        f"{first.syscall}->{second.syscall}" for first, second in zip(ops, ops[1:])
    )

    summary = CaseSummary(
        path=str(path),
        size=len(data),
        op_count=len(ops),
        trailing_bytes=min(len(data), MAX_OPS * OP_SIZE) % OP_SIZE,
        syscalls=dict(sorted(syscall_counts.items())),
        seq2=dict(sorted(seq_counts.items())),
    )

    if target is not None:
        try:
            result = subprocess.run(
                [str(target), str(path)],
                stdin=subprocess.DEVNULL,
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
                timeout=timeout,
                check=False,
            )
            summary.exit_code = result.returncode
            summary.timed_out = False
        except subprocess.TimeoutExpired:
            summary.exit_code = None
            summary.timed_out = True

    return summary


def aggregate(summaries: Iterable[CaseSummary]) -> dict[str, object]:
    summaries = list(summaries)
    syscall_totals: Counter[str] = Counter()
    seq_totals: Counter[str] = Counter()
    for summary in summaries:
        syscall_totals.update(summary.syscalls)
        seq_totals.update(summary.seq2)

    replayed = [
        summary
        for summary in summaries
        if summary.exit_code is not None or summary.timed_out
    ]
    return {
        "files": len(summaries),
        "total_bytes": sum(summary.size for summary in summaries),
        "total_operations": sum(summary.op_count for summary in summaries),
        "unique_syscalls": len(syscall_totals),
        "unique_seq2": len(seq_totals),
        "syscalls": dict(sorted(syscall_totals.items())),
        "seq2": dict(sorted(seq_totals.items())),
        "replay": {
            "files": len(replayed),
            "timeouts": sum(1 for summary in replayed if summary.timed_out),
            "nonzero_exits": sum(
                1
                for summary in replayed
                if summary.exit_code not in (None, 0) and not summary.timed_out
            ),
        },
    }


def write_csv(path: Path, summaries: Sequence[CaseSummary]) -> None:
    with path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(
            handle,
            fieldnames=[
                "path",
                "size",
                "op_count",
                "trailing_bytes",
                "unique_syscalls",
                "unique_seq2",
                "exit_code",
                "timed_out",
            ],
        )
        writer.writeheader()
        for summary in summaries:
            writer.writerow(
                {
                    "path": summary.path,
                    "size": summary.size,
                    "op_count": summary.op_count,
                    "trailing_bytes": summary.trailing_bytes,
                    "unique_syscalls": len(summary.syscalls),
                    "unique_seq2": len(summary.seq2),
                    "exit_code": "" if summary.exit_code is None else summary.exit_code,
                    "timed_out": "" if summary.timed_out is None else summary.timed_out,
                }
            )


def parse_args(argv: Sequence[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Decode PoC_one testcases and summarize AFL queue coverage signals."
    )
    parser.add_argument(
        "paths",
        nargs="+",
        type=Path,
        help="AFL output directories, queue directories, or individual testcase files.",
    )
    parser.add_argument(
        "--target",
        type=Path,
        help="Optional poc_one binary to replay each testcase as a smoke check.",
    )
    parser.add_argument(
        "--timeout",
        type=float,
        default=1.0,
        help="Per-testcase replay timeout in seconds when --target is used (default: 1.0).",
    )
    parser.add_argument(
        "--format",
        choices=("text", "json"),
        default="text",
        help="Output format for the aggregate report (default: text).",
    )
    parser.add_argument("--csv", type=Path, help="Optional per-file CSV output path.")
    parser.add_argument(
        "--details",
        action="store_true",
        help="Include per-file details in JSON output.",
    )
    return parser.parse_args(argv)


def main(argv: Sequence[str] | None = None) -> int:
    args = parse_args(sys.argv[1:] if argv is None else argv)

    target = args.target.resolve() if args.target is not None else None
    if target is not None and not target.exists():
        print(f"error: target does not exist: {target}", file=sys.stderr)
        return 2

    try:
        files = list(afl_queue_files(args.paths))
    except FileNotFoundError as exc:
        print(f"error: path does not exist: {exc}", file=sys.stderr)
        return 2

    summaries = [summarize_case(path, target, args.timeout) for path in files]
    report = aggregate(summaries)

    if args.csv is not None:
        write_csv(args.csv, summaries)

    if args.format == "json":
        payload = dict(report)
        if args.details:
            payload["cases"] = [asdict(summary) for summary in summaries]
        print(json.dumps(payload, indent=2, sort_keys=True))
        return 0

    print("PoC_one queue analysis")
    print(f"  files:            {report['files']}")
    print(f"  total bytes:      {report['total_bytes']}")
    print(f"  total operations: {report['total_operations']}")
    print(f"  unique syscalls:  {report['unique_syscalls']} / {len(SYSCALLS)}")
    print(f"  unique seq2:      {report['unique_seq2']}")
    replay = report["replay"]
    assert isinstance(replay, dict)
    if replay["files"]:
        print("  replay:")
        print(f"    files:          {replay['files']}")
        print(f"    timeouts:       {replay['timeouts']}")
        print(f"    nonzero exits:  {replay['nonzero_exits']}")
    print("\nSyscall totals:")
    syscalls = report["syscalls"]
    assert isinstance(syscalls, dict)
    for name, count in syscalls.items():
        print(f"  {name:12s} {count}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
