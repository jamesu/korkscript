#!/usr/bin/env bash
set -euo pipefail

# Config via env, with sane defaults
VERIFY_BRANCH="${VERIFY_BRANCH:-verify_branch}"
CURRENT_BUILD_DIR="${CURRENT_BUILD_DIR:-test_build}"
VERIFY_BUILD_DIR="${VERIFY_BUILD_DIR:-verify_build}"
TEST_SCRIPTS_DIR="${TEST_SCRIPTS_DIR:-test/parser_samples}"
CS_BIN_NAME="${CS_BIN_NAME:-csprintast}"

ROOT_DIR="$(git rev-parse --show-toplevel)"
cd "$ROOT_DIR"

echo "Using VERIFY_BRANCH=$VERIFY_BRANCH"
echo "Using TEST_SCRIPTS_DIR=$TEST_SCRIPTS_DIR"

# Make sure we have that branch locally
git fetch origin "$VERIFY_BRANCH":"$VERIFY_BRANCH"

# Add worktree for verify_branch
VERIFY_WORKTREE_DIR="$ROOT_DIR/../verify_branch"
git worktree add "$VERIFY_WORKTREE_DIR" "$VERIFY_BRANCH"

cleanup() {
  echo "Cleaning up worktree..."
  #git worktree remove "$VERIFY_WORKTREE_DIR" --force || true
}
trap cleanup EXIT

# Build current branch csprintast
echo "Building current branch... ($CURRENT_BUILD_DIR)"
"$ROOT_DIR/tools/build_ast_print.sh" "$CURRENT_BUILD_DIR"

# Build verify_branch csprintast
echo "Building verify_branch... ($VERIFY_WORKTREE_DIR}"
(
  cd "$VERIFY_WORKTREE_DIR"
  "$ROOT_DIR/tools/build_ast_print.sh" "$VERIFY_BUILD_DIR"
)

ORIG_BIN="$VERIFY_WORKTREE_DIR/$VERIFY_BUILD_DIR/$CS_BIN_NAME"
CURR_BIN="$ROOT_DIR/$CURRENT_BUILD_DIR/$CS_BIN_NAME"

echo "Original binary: $ORIG_BIN"
echo "Current  binary: $CURR_BIN"

"$ROOT_DIR/tools/run_ast_regression.sh" "$ORIG_BIN" "$CURR_BIN" "$TEST_SCRIPTS_DIR"
