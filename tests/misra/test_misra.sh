#!/bin/bash -e

INPUT1=$1

echo $INPUT1

git clone https://github.com/danmar/cppcheck.git || true
cd cppcheck
git checkout 1584e6236758d54b0d246d41771d67338e0bd41b
make -j4
cd ../../../

if [ "$INPUT1" != "safety-only" ]; then
  # whole panda code
  tests/misra/cppcheck/cppcheck --dump --inline-suppr --enable=all board/main.c 2>/tmp/misra/cppcheck_output.txt || true
  python tests/misra/cppcheck/addons/misra.py board/main.c.dump 2>/tmp/misra/misra_output.txt || true
fi

# just safety
tests/misra/cppcheck/cppcheck --dump --inline-suppr --enable=all board/safety.h 2>/tmp/misra/cppcheck_safety_output.txt || true
python tests/misra/cppcheck/addons/misra.py board/safety.h.dump 2>/tmp/misra/misra_safety_output.txt || true
