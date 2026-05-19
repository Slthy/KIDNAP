#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import os
import re
from collections import Counter
from pathlib import Path

GRAPH_FUNC_RE = re.compile(r"([A-Za-z_][A-Za-z0-9_]*)\s*(?:\(|\{)")
PLAIN_FUNC_RE = re.compile(r":\s+([A-Za-z_][A-Za-z0-9_]*)\s+<-")
SYSCALL_EVENT_RE = re.compile(r":\s+sys_(?:enter|exit)_([A-Za-z0-9_]+):")
PREFIXES = {
    "cgroup": ("cgroup_",),
    "vfs": ("vfs_", "do_sys_openat2", "path_lookupat"),
    "proc_sysfs": ("proc_", "kernfs_"),
    "socket_netlink": ("sock_", "unix_", "tcp_", "netlink_"),
}
SYSCALL_MARKERS = ("__x64_sys_", "__arm64_sys_", "do_sys_", "ksys_")


def parse_trace(path: Path) -> list[str]:
    funcs: list[str] = []
    for line in path.read_text(errors="replace").splitlines():
        match = SYSCALL_EVENT_RE.search(line) or PLAIN_FUNC_RE.search(line) or GRAPH_FUNC_RE.search(line)
        if match:
            funcs.append(match.group(1))
    return funcs


def summarize(funcs: list[str], trace_mode: str = "unknown") -> dict[str, object]:
    unique = sorted(set(funcs))
    pairs = sorted(set(zip(funcs, funcs[1:])))
    by_prefix = {
        name: sorted(fn for fn in unique if fn.startswith(prefixes))
        for name, prefixes in PREFIXES.items()
    }
    syscalls = sorted(
        unique if trace_mode == "syscalls" else (
            fn for fn in unique if fn.startswith(SYSCALL_MARKERS)
        )
    )
    return {
        "trace_mode": trace_mode,
        "event_count": len(funcs),
        "unique_events": len(unique),
        "unique_event_pairs": len(pairs),
        "function_events": len(funcs),
        "unique_functions": len(unique),
        "unique_function_pairs": len(pairs),
        "unique_syscall_like_functions": len(syscalls),
        "subsystems": {name: len(values) for name, values in by_prefix.items()},
        "events": unique,
        "event_pairs": [list(pair) for pair in pairs],
        "functions": unique,
        "function_pairs": [list(pair) for pair in pairs],
        "syscall_like_functions": syscalls,
        "subsystem_functions": by_prefix,
        "top_events": Counter(funcs).most_common(20),
        "top_functions": Counter(funcs).most_common(20),
    }


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("trace", type=Path)
    parser.add_argument("--trace-mode", default=os.environ.get("TRACE_PROFILE", "unknown"))
    parser.add_argument("--compact", action="store_true")
    args = parser.parse_args()
    data = summarize(parse_trace(args.trace), args.trace_mode)
    if args.compact:
        data.pop("events")
        data.pop("event_pairs")
        data.pop("functions")
        data.pop("function_pairs")
        data.pop("syscall_like_functions")
        data.pop("subsystem_functions")
    print(json.dumps(data, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
