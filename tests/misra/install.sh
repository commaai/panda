#!/bin/bash
set -e

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
: "${CPPCHECK_DIR:=$DIR/cppcheck/}"

if [ ! -d "$CPPCHECK_DIR" ]; then
  git clone https://github.com/danmar/cppcheck.git $CPPCHECK_DIR
fi

cd $CPPCHECK_DIR
# 2.13.0 plus some fixes
# 2.13.0 plus some fixes
VERS="f6b538e855f0bacea33c4074664628024ef39dc6"
git fetch --all
git checkout $VERS
#make clean
make MATCHCOMPILTER=yes CXXFLAGS="-O2" -j8
