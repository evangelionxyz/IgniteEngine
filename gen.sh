#!/usr/bin/env bash
set -euo pipefail

# Lightweight bootstrap for CMake configure on Linux.
# Optional dependencies (uncomment to install):
# sudo apt update
# sudo apt install -y build-essential ninja-build cmake git pkg-config \
#     libxkbcommon-dev libwayland-dev xorg-dev zlib1g-dev libfmt-dev \
#     libvulkan1 mesa-vulkan-drivers vulkan-utils

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &>/dev/null && pwd)"
ROOT_DIR="${SCRIPT_DIR}"

pushd "${ROOT_DIR}" >/dev/null
# python3 scripts/setup.py --with-build -c Debug
popd >/dev/null

echo "Done. To build again: cmake --build build --config Debug"