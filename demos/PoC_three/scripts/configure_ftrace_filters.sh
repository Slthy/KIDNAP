#!/usr/bin/env bash
set -euo pipefail

TRACE_DIR="${TRACE_DIR:-/sys/kernel/debug/tracing}"
TRACE_PROFILE="${1:-${TRACE_PROFILE:-syscalls}}"
if [[ $EUID -ne 0 ]]; then
  echo "configure_ftrace_filters.sh must run as root on the outer VM." >&2
  exit 1
fi
[[ -d "$TRACE_DIR" ]] || { echo "tracefs not found at $TRACE_DIR" >&2; exit 1; }

# Always reset both mechanisms before configuring the requested profile.
echo nop > "$TRACE_DIR/current_tracer"
: > "$TRACE_DIR/set_ftrace_filter" 2>/dev/null || true
echo 0 > "$TRACE_DIR/events/enable" 2>/dev/null || true

case "$TRACE_PROFILE" in
  off)
    echo "Configured tracing off."
    ;;
  syscalls)
    [[ -d "$TRACE_DIR/events/syscalls" ]] || {
      echo "syscall tracepoints not available under $TRACE_DIR/events/syscalls" >&2
      exit 1
    }
    syscall_events=(
      sys_enter_openat
      sys_enter_openat2
      sys_enter_newfstatat
      sys_enter_statx
      sys_enter_mkdirat
      sys_enter_unlinkat
      sys_enter_renameat2
      sys_enter_clone
      sys_enter_clone3
      sys_enter_execve
      sys_enter_execveat
      sys_enter_socket
      sys_enter_bind
      sys_enter_listen
      sys_enter_connect
      sys_enter_accept
      sys_enter_accept4
      sys_enter_sendto
      sys_enter_recvfrom
      sys_enter_sendmsg
      sys_enter_recvmsg
      sys_enter_mount
      sys_enter_umount2
      sys_enter_readlinkat
    )
    configured=0
    for event in "${syscall_events[@]}"; do
      if [[ -e "$TRACE_DIR/events/syscalls/$event/enable" ]]; then
        echo 1 > "$TRACE_DIR/events/syscalls/$event/enable"
        configured=$((configured + 1))
      fi
    done
    echo "Configured $configured focused syscall tracepoints for low-overhead replay."
    ;;
  focused|broad)
    [[ -r "$TRACE_DIR/available_filter_functions" ]] || {
      echo "available_filter_functions not readable under $TRACE_DIR" >&2
      exit 1
    }
    echo function > "$TRACE_DIR/current_tracer"
    case "$TRACE_PROFILE" in
      focused)
        entries=(
          '^do_sys_openat2$|do_sys_openat2'
          '^vfs_|vfs_*'
          '^kernfs_|kernfs_*'
          '^cgroup_|cgroup_*'
          '^unix_|unix_*'
          '^sock_|sock_*'
          '^netlink_|netlink_*'
          '^copy_process$|copy_process'
          '^do_exit$|do_exit'
        )
        ;;
      broad)
        entries=(
          '^do_sys_openat2$|do_sys_openat2'
          '^vfs_|vfs_*'
          '^kernfs_|kernfs_*'
          '^cgroup_|cgroup_*'
          '^proc_|proc_*'
          '^security_|security_*'
          '^unix_|unix_*'
          '^sock_|sock_*'
          '^tcp_|tcp_*'
          '^netlink_|netlink_*'
          '^copy_process$|copy_process'
          '^do_exit$|do_exit'
          '^do_execveat_common$|do_execveat_common'
        )
        ;;
    esac
    configured=0
    for entry in "${entries[@]}"; do
      regex="${entry%%|*}"
      glob="${entry#*|}"
      if grep -Eq "$regex" "$TRACE_DIR/available_filter_functions"; then
        printf '%s\n' "$glob" >> "$TRACE_DIR/set_ftrace_filter"
        configured=$((configured + 1))
      else
        echo "warning: no traceable functions matched $glob" >&2
      fi
    done
    echo "Configured $configured ftrace filter patterns for profile '$TRACE_PROFILE'."
    ;;
  *)
    echo "unknown TRACE_PROFILE: $TRACE_PROFILE (expected off, syscalls, focused, or broad)" >&2
    exit 2
    ;;
esac
