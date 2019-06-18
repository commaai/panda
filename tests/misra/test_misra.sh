#!/bin/bash -e

INPUT1=$1

echo $INPUT1

git clone https://github.com/danmar/cppcheck.git || true
cd cppcheck
#git checkout 1584e6236758d54b0d246d41771d67338e0bd41b
git fetch
git checkout 44d6066c6fad32e2b0332b3f2b24bd340febaef8
make -j4
cd ../../../

if [ "$INPUT1" != "safety-only" ]; then
  # whole panda code
  tests/misra/cppcheck/cppcheck --dump --enable=all --inline-suppr board/main.c 2>/tmp/misra/cppcheck_output.txt || true
  python tests/misra/cppcheck/addons/misra.py board/main.c.dump 2>/tmp/misra/misra_output.txt || true
fi

# violations in safety files
(cat /tmp/misra/misra_output.txt | grep safety) > /tmp/misra/misra_safety_output.txt
(cat /tmp/misra/cppcheck_output.txt | grep safety) > /tmp/misra/cppcheck_safety_output.txt
