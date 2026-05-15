#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

echo "[*] Resetting PoC 2 state..."

# Remove AFL output directories
rm -rf out-baseline
rm -rf out-feature-feedback
rm -rf out-configlet-feedback
rm -rf out-round1
rm -rf out-round2

# Remove generated feedback logs
rm -f feedback-baseline.log
rm -f feedback-feature.log
rm -f feedback-configlet.log
rm -f feedback-round1.log
rm -f feedback-round2.log

# Remove generated seed dirs, but keep original seeds/
rm -rf seeds-round2
rm -rf seeds-feature
rm -rf seeds-configlet

# Remove generated analysis/plot outputs
rm -f footprint.csv
rm -f footprint_growth.csv
rm -f footprint_growth.png
rm -f analysis.txt

# Remove crashes/hangs summaries if you create them later
rm -f crashes.txt
rm -f hangs.txt

# Optional: rebuild target from clean state
if [[ "${1:-}" == "--build" ]]; then
    echo "[*] Cleaning and rebuilding target..."
    make clean
    make
fi

echo "[+] Reset complete."