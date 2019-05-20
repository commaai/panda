#!/bin/bash -e
git clone https://github.com/danmar/cppcheck.git || true
cd cppcheck
make -j4
cd ../../../
tests/misra/cppcheck/cppcheck --dump board/main.c
python tests/misra/cppcheck/addons/misra.py board/main.c.dump 2>/tmp/misra/output.txt  || true
