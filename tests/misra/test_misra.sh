#!/bin/bash -e
git clone https://github.com/danmar/cppcheck.git || true
cd cppcheck
git checkout 1584e6236758d54b0d246d41771d67338e0bd41b
make -j4
cd ../../../
tests/misra/cppcheck/cppcheck --dump board/main.c
python tests/misra/cppcheck/addons/misra.py board/main.c.dump 2>/tmp/misra/output.txt || true
