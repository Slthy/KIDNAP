#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
UNIT_DIR="${UNIT_DIR:-/etc/systemd/system}"

if [[ $EUID -ne 0 ]]; then
  echo "install_units.sh must run as root inside the disposable VM." >&2
  exit 1
fi

install -m 0644 "$ROOT_DIR"/units/* "$UNIT_DIR"/
mkdir -p /tmp/systemctl-fuzz
systemctl daemon-reload
echo "Installed fuzz units into $UNIT_DIR"
