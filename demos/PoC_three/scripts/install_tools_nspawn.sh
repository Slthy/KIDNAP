#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
exec "$ROOT_DIR/scripts/nspawn_exec.sh" bash -lc '
  set -e
  apt-get update -o APT::Update::Error-Mode=any
  apt-get install -y systemd dbus python3 make gcc afl++
'
