#!/usr/bin/env bash
set -euo pipefail

# Usage: ./tools/build_testrunner.sh [build_dir]
# Defaults to "test_build" if not provided.

BUILD_DIR="${1:-${CURRENT_BUILD_DIR:-test_build}}"
CATCH2_ASAN="${CATCH2_ASAN:-0}"
echo "BUILD DIR IS ${BUILD_DIR}"

cpu_count() {
  if command -v getconf >/dev/null 2>&1; then
    getconf _NPROCESSORS_ONLN 2>/dev/null && return 0
  fi
  if command -v sysctl >/dev/null 2>&1; then
    sysctl -n hw.ncpu 2>/dev/null && return 0
  fi
  echo 4
}

mkdir -p "$BUILD_DIR"
if [ "$CATCH2_ASAN" = "1" ]; then
  ASAN_FLAGS="-g -O1 -fno-omit-frame-pointer -fsanitize=address"
  if [ -n "${CFLAGS:-}" ]; then
    export CFLAGS="${CFLAGS} ${ASAN_FLAGS}"
  else
    export CFLAGS="${ASAN_FLAGS}"
  fi
  if [ -n "${CXXFLAGS:-}" ]; then
    export CXXFLAGS="${CXXFLAGS} ${ASAN_FLAGS}"
  else
    export CXXFLAGS="${ASAN_FLAGS}"
  fi
  cmake -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=RelWithDebInfo
else
  cmake -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release
fi
cmake --build "$BUILD_DIR" -- -j"$(cpu_count)"
