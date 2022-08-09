#!/bin/bash
set -e

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

cd $DIR
if [ ! -d cppcheck/ ]; then
  git clone https://github.com/danmar/cppcheck.git
fi

cd cppcheck
git fetch
git checkout e1cff1d1ef92f6a1c6962e0e4153b7353ccad04c
make clean
make MATCHCOMPILTER=yes CXXFLAGS="-O2" -j8
