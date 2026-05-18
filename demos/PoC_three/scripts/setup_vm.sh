#!/usr/bin/env bash
set -euo pipefail

cat <<'EOF'
Nested KVM is not required for PoC_three. Use systemd-nspawn on the outer VM:

  sudo ./scripts/setup_nspawn.sh
  sudo ./scripts/start_nspawn.sh

In another terminal on the outer VM:

  sudo ./scripts/configure_ftrace_filters.sh
  ./scripts/install_units_nspawn.sh
  ./scripts/run_baseline.sh

The nspawn machine runs systemd and the harnesses; ftrace remains on the outer
VM because nspawn shares the host kernel.
EOF
