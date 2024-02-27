#!/bin/bash
set -e

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
: "${CPPCHECK_DIR:=$DIR/cppcheck/}"

if [ ! -d "$CPPCHECK_DIR" ]; then
  git clone https://github.com/danmar/cppcheck.git $CPPCHECK_DIR
fi

cd $CPPCHECK_DIR

VERS="2.13.0"
git fetch --all --tags
git checkout $VERS
git cherry-pick -n f6b538e855f0bacea33c4074664628024ef39dc6 b11b42087ff29569bc3740f5aa07eb6616ea4f63

#make clean
make MATCHCOMPILTER=yes CXXFLAGS="-O2" -j8
