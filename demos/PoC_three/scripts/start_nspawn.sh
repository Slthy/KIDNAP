#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MACHINE="${MACHINE:-systemctl-fuzz}"
ROOTFS="${ROOTFS:-/var/lib/machines/$MACHINE}"

if [[ $EUID -ne 0 ]]; then
  echo "start_nspawn.sh must run as root on the outer VM." >&2
  exit 1
fi

exec systemd-nspawn \
  --resolv-conf=copy-uplink \
  --machine="$MACHINE" \
  -D "$ROOTFS" \
  --bind="$ROOT_DIR:/opt/systemctl-ftrace-poc" \
  -b
