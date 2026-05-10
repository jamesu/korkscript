#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(git rev-parse --show-toplevel)"
cd "$ROOT_DIR"

CURRENT_BUILD_DIR="${CURRENT_BUILD_DIR:-asan2_build}"
CATCH2_ASAN="${CATCH2_ASAN:-1}"
export CURRENT_BUILD_DIR CATCH2_ASAN

"$ROOT_DIR/tools/build_testrunner.sh" "$CURRENT_BUILD_DIR"
"$ROOT_DIR/tools/run_catch2_tests.sh" "$@"
