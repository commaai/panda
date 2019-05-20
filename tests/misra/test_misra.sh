#!/bin/bash -e
git clone https://github.com/danmar/cppcheck.git || true
cd cppcheck
make
cd ../../../
ls
tests/misra/cppcheck/cppcheck --dump board/main.c
python tests/misra/cppcheck/addons/misra.py board/main.c.dump
