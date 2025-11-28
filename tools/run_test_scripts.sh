#!/usr/bin/env bash
set -euo pipefail

CURRENT_BUILD_DIR="${CURRENT_BUILD_DIR:-test_build}"
TEST_SCRIPTS_DIR="${TEST_SCRIPTS_DIR:-test/script_tests}"
CS_BIN_NAME="${CS_BIN_NAME:-torquetest}"

while IFS= read -r -d '' script; do
  rel="${script#$TEST_SCRIPTS_DIR/}"
  echo "== Testing: $rel =="

  "$CS_BIN_NAME" "$script"
done < <(find "$TEST_SCRIPTS_DIR" -type f \( -name '*.cs' -o -name '*.tscript' \) -print0)
