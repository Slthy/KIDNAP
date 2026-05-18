#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 2 ]]; then
  echo "usage: $0 TRACE_OUT -- COMMAND [ARGS...]" >&2
  exit 2
fi

TRACE_OUT="$1"
shift
[[ "${1:-}" == "--" ]] || { echo "missing -- separator" >&2; exit 2; }
shift

TRACE_DIR="${TRACE_DIR:-/sys/kernel/debug/tracing}"
if [[ $EUID -ne 0 ]]; then
  echo "collect_ftrace.sh must run as root on the outer VM that owns tracefs." >&2
  exit 1
fi
[[ -d "$TRACE_DIR" ]] || { echo "tracefs not found at $TRACE_DIR" >&2; exit 1; }

# Respect the profile already configured by configure_ftrace_filters.sh.
# Override only when the caller explicitly requests a tracer.
if [[ -n "${TRACER:-}" ]]; then
  echo "$TRACER" > "$TRACE_DIR/current_tracer"
fi
echo 0 > "$TRACE_DIR/tracing_on"
: > "$TRACE_DIR/trace"
echo 1 > "$TRACE_DIR/tracing_on"
REPLAY_TIMEOUT="${REPLAY_TIMEOUT:-15}"
set +e
if [[ "$REPLAY_TIMEOUT" == "0" ]]; then
  "$@"
else
  timeout --foreground --signal=TERM --kill-after=2 "${REPLAY_TIMEOUT}s" "$@"
fi
status=$?
set -e
echo 0 > "$TRACE_DIR/tracing_on"
cat "$TRACE_DIR/trace" > "$TRACE_OUT"
exit "$status"
