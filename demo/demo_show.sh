#!/bin/bash

set -e

QUEUE_DIR="out/default/queue"
TARGET="./mini"

[ -d "$QUEUE_DIR" ] || { echo "Run AFL++ first: no $QUEUE_DIR found"; exit 1; }
[ -x "$TARGET" ] || { echo "Target $TARGET not found or not executable"; exit 1; }

touch seen.txt

for f in "$QUEUE_DIR"/id:*; do
  [ -e "$f" ] || continue

  strace -f "$TARGET" < "$f" 2> trace.txt

  grep -oE '^[a-zA-Z0-9_]+' trace.txt | sort -u > current.txt || true

  NEW=$(comm -13 seen.txt current.txt)

  if [ -n "$NEW" ]; then
    echo "[+] NEW KERNEL BEHAVIOR from $f:"
    echo "$NEW"
    echo
    cat seen.txt current.txt | sort -u > tmp && mv tmp seen.txt
  fi
done

echo "Final observed kernel behavior:"
cat seen.txt