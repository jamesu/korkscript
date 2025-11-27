#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -lt 3 ]; then
  echo "Usage: $0 <original_csprintast> <current_csprintast> <scripts_dir>" >&2
  exit 1
fi

ORIG_BIN="$1"
CURR_BIN="$2"
SCRIPTS_DIR="$3"

TMP_ROOT="${TMP_ROOT:-$(mktemp -d)}"
trap 'rm -rf "$TMP_ROOT"' EXIT

STATUS=0

# tweak extensions as needed
while IFS= read -r -d '' script; do
  rel="${script#$SCRIPTS_DIR/}"
  echo "== Testing: $rel =="

  out_orig="$TMP_ROOT/${rel}.orig.ast"
  out_curr="$TMP_ROOT/${rel}.curr.ast"
  out_orig_bytecode="$TMP_ROOT/${rel}.orig.bc"
  out_curr_bytecode="$TMP_ROOT/${rel}.curr.bc"
  mkdir -p "$(dirname "$out_orig")"
  mkdir -p "$(dirname "$out_curr")"

  "$ORIG_BIN" "$script" > "$out_orig"
  "$CURR_BIN" "$script" > "$out_curr"
  "$ORIG_BIN" "$script" -b -x > "$out_orig_bytecode"
  "$CURR_BIN" "$script" -b > "$out_curr_bytecode"

  if ! diff -u "$out_orig" "$out_curr" > "$TMP_ROOT/${rel}.diff" 2>&1; then
    echo "!! AST mismatch for $rel !!"
    cat "$TMP_ROOT/${rel}.diff"
    STATUS=1
  else
    echo "PASS AST"
  fi

  if ! diff -u "$out_orig_bytecode" "$out_curr_bytecode" > "$TMP_ROOT/${rel}.diff" 2>&1; then
    echo "!! AST mismatch for $rel !!"
    cat "$TMP_ROOT/${rel}.diff"
    STATUS=1
  else
    echo "PASS BYTECODE"
  fi

  # cleanup: these files get LARGE
  rm "$out_orig"
  rm "$out_curr"
  rm "$TMP_ROOT/${rel}.diff"
done < <(find "$SCRIPTS_DIR" -type f \( -name '*.cs' -o -name '*.tscript' \) -print0)

exit "$STATUS"
