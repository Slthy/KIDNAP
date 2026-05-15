#!/usr/bin/env bash
set -euo pipefail

rm -rf out-baseline feedback-baseline.log

export AFL_SKIP_CPUFREQ=1
export AFL_I_DONT_CARE_ABOUT_MISSING_CRASHES=1
export CONFIGLET_FEEDBACK_FILE="$PWD/feedback-baseline.log"

afl-fuzz -i seeds -o out-baseline -- ./configlet_sim @@