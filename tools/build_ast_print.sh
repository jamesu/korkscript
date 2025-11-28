#!/usr/bin/env bash
set -euo pipefail

# Usage: ./tools/build_csprintast.sh [build_dir]
# Defaults to "build" if not provided.

BUILD_DIR="${1:-build}"
echo "BUILD DIR IS ${BUILD_DIR}"
CS_BIN_NAME="${CS_BIN_NAME:-csprintast}"

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -- -j"$(nproc)"
