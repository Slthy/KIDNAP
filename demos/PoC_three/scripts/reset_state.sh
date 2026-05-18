#!/usr/bin/env bash
set -euo pipefail

systemctl stop fuzz-ok.service fuzz-fail.service fuzz-sleep.service fuzz-fs.service \
  fuzz-timer.timer fuzz-socket.socket fuzz-path.path 2>/dev/null || true
systemctl unmask fuzz-ok.service 2>/dev/null || true
systemctl disable fuzz-ok.service 2>/dev/null || true
systemctl reset-failed 2>/dev/null || true
rm -rf /tmp/systemctl-fuzz
mkdir -p /tmp/systemctl-fuzz
