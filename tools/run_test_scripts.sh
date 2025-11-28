#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(git rev-parse --show-toplevel)"
cd "$ROOT_DIR"

CURRENT_BUILD_DIR="${CURRENT_BUILD_DIR:-test_build}"
TEST_SCRIPTS_DIR="${TEST_SCRIPTS_DIR:-test/script_tests}"
TEST_RUNNER_BIN_NAME="${TEST_RUNNER_BIN_NAME:-testrunner}"
CURRENT_BUILD_DIR="${CURRENT_BUILD_DIR:-test_build}"
TEST_RUNNER_BIN="$ROOT_DIR/$CURRENT_BUILD_DIR/$TEST_RUNNER_BIN_NAME"

while IFS= read -r -d '' script; do
  rel="${script#$TEST_SCRIPTS_DIR/}"
  echo "== Testing: $rel =="

  "$TEST_RUNNER_BIN" "$script"
done < <(find "$TEST_SCRIPTS_DIR" -type f \( -name '*.cs' -o -name '*.tscript' \) -print0)
