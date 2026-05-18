#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TARGET_HARNESS="${NSPAWN_TARGET_HARNESS:-systemctl_harness}"
if [[ $# -ne 1 ]]; then
  echo "usage: $0 TESTCASE" >&2
  exit 2
fi

case_path="$(realpath "$1")"
case "$case_path" in
  "$ROOT_DIR"/*) rel_case="${case_path#$ROOT_DIR/}" ;;
  *) echo "testcase must live under $ROOT_DIR so it is visible in the bind mount" >&2; exit 1 ;;
esac

exec "$ROOT_DIR/scripts/nspawn_exec.sh" \
  "/opt/systemctl-ftrace-poc/$TARGET_HARNESS" \
  "/opt/systemctl-ftrace-poc/$rel_case"
