#!/usr/bin/env bash
set -euo pipefail

# Local coverage report into /tmp
OUT_DIR="/tmp/llmengine-coverage"
mkdir -p "$OUT_DIR"

echo "Generating coverage into $OUT_DIR"

if command -v gcovr >/dev/null 2>&1; then
  gcovr -r .. --exclude-directories build_examples --xml "$OUT_DIR/coverage.xml"
  gcovr -r .. --exclude-directories build_examples --html-details "$OUT_DIR/index.html"
  echo "gcovr reports written to $OUT_DIR"
elif command -v llvm-profdata >/dev/null 2>&1 && command -v llvm-cov >/dev/null 2>&1; then
  PROFRAW_DIR="${OUT_DIR}/profraw"
  mkdir -p "$PROFRAW_DIR"
  echo "Collect .profraw with LLVM_PROFILE_FILE env during test runs."
  echo "Then run llvm-profdata merge and llvm-cov show as needed."
else
  echo "Neither gcovr nor llvm-cov found. Please install one to generate coverage."
  exit 1
fi

echo "Done."


