#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

rm -rf out-configlet-feedback feedback-configlet.log

export AFL_SKIP_CPUFREQ="${AFL_SKIP_CPUFREQ:-1}"
export AFL_I_DONT_CARE_ABOUT_MISSING_CRASHES="${AFL_I_DONT_CARE_ABOUT_MISSING_CRASHES:-1}"
export CONFIGLET_FEEDBACK_FILE="$PWD/feedback-configlet.log"

make clean
make feedback

afl-fuzz \
  -i seeds \
  -o out-configlet-feedback \
  -x dict/configlet.dict \
  -- ./configlet_sim @@
