#!/usr/bin/env bash
set -e

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
: "${CPPCHECK_DIR:=$DIR/cppcheck/}"

if [ ! -d "$CPPCHECK_DIR" ]; then
  git clone https://github.com/danmar/cppcheck.git $CPPCHECK_DIR
fi

cd $CPPCHECK_DIR

VERS="2.16.0"
git fetch --all --tags --force
git checkout $VERS

# Apply patch for enforcing MISRA compliant Boolean values
# This patch adds support for catching integer-to-bool assignments
# Ref: https://github.com/danmar/cppcheck/pull/7110
PATCH_FILE="$DIR/0001-feat-enforcing-MISRA-compliant-Boolean-values.patch"
if [ -f "$PATCH_FILE" ]; then
  git apply "$PATCH_FILE" || echo "Warning: Could not apply patch, it may already be applied"
fi

#make clean
make MATCHCOMPILTER=yes CXXFLAGS="-O2" -j8
