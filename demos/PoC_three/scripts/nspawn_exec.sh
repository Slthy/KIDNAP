#!/usr/bin/env bash
set -euo pipefail

MACHINE="${MACHINE:-systemctl-fuzz}"
if [[ $# -eq 0 ]]; then
  echo "usage: $0 COMMAND [ARGS...]" >&2
  exit 2
fi

quoted="cd /opt/systemctl-ftrace-poc && exec"
for arg in "$@"; do
  printf -v q ' %q' "$arg"
  quoted+="$q"
done

if [[ $EUID -eq 0 ]]; then
  exec machinectl --quiet shell "$MACHINE" /bin/bash -lc "$quoted"
else
  exec sudo machinectl --quiet shell "$MACHINE" /bin/bash -lc "$quoted"
fi
