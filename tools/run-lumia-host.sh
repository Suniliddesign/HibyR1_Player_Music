#!/usr/bin/env bash
set -euo pipefail
repo_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_dir/sonix-player"
# Host resource paths are relative to this working directory. Supply the real
# R1 model, fonts and translations so the simulator uses the 480x800 layout.
mkdir -p usr/resource
cp -a "$repo_dir/sonix-packer/assets/R1/usr/resource/." usr/resource/
mkdir -p "$repo_dir/test-music"
export SONIX_SD_ROOT="${SONIX_SD_ROOT:-$repo_dir/test-music}"
mkdir -p "$SONIX_SD_ROOT/.local"
export SONIX_CONFIG="${SONIX_CONFIG:-$SONIX_SD_ROOT/.local/device_config.ini}"
make host -j"${JOBS:-2}"
exec ./sonix_player_host
