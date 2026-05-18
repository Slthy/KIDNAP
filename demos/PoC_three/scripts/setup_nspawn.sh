#!/usr/bin/env bash
set -euo pipefail

MACHINE="${MACHINE:-systemctl-fuzz}"
ROOTFS="${ROOTFS:-/var/lib/machines/$MACHINE}"
RELEASE="${RELEASE:-jammy}"
MIRROR="${MIRROR:-http://archive.ubuntu.com/ubuntu/}"
NSPAWN=(systemd-nspawn --resolv-conf=copy-uplink -D "$ROOTFS" --)

if [[ $EUID -ne 0 ]]; then
  echo "setup_nspawn.sh must run as root on the outer VM." >&2
  exit 1
fi

apt-get update -o APT::Update::Error-Mode=any
apt-get install -y systemd-container debootstrap
mkdir -p "$ROOTFS"
if [[ ! -x "$ROOTFS/bin/bash" ]]; then
  debootstrap "$RELEASE" "$ROOTFS" "$MIRROR"
fi

# Fresh debootstrap images often start with only the `main` component enabled.
# AFL++ is packaged in Ubuntu's `universe`, so make that component explicit.
if [[ -f "$ROOTFS/etc/apt/sources.list" ]] && ! grep -Eq '^[^#].*\buniverse\b' "$ROOTFS/etc/apt/sources.list"; then
  sed -i '/^[[:space:]]*deb / s/ main\([[:space:]]*$\)/ main universe\1/' "$ROOTFS/etc/apt/sources.list"
fi

# Do not pass the host's stub resolver (127.0.0.53) into the container. nspawn's
# copy-uplink mode injects the real upstream resolver configuration for each run.
"${NSPAWN[@]}" apt-get update -o APT::Update::Error-Mode=any
"${NSPAWN[@]}" apt-get install -y systemd dbus python3 make gcc afl++
"${NSPAWN[@]}" bash -lc 'command -v systemctl && command -v gcc && command -v make && command -v afl-fuzz'
echo "Prepared nspawn rootfs at $ROOTFS"
