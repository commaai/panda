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
git -c user.name=a -c user.email="a@b.c" cherry-pick 7199dde1618b5166735f07619dcdb9f4eafdb557
make clean
make MATCHCOMPILTER=yes CXXFLAGS="-O2" -j8
