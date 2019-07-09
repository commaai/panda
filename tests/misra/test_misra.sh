#!/bin/bash -e

git clone https://github.com/danmar/cppcheck.git || true
cd cppcheck
git fetch
git checkout 862c4ef87b109ae86c2d5f12769b7c8d199f35c5
make -j4
cd ../../../

# panda code
tests/misra/cppcheck/cppcheck -DPANDA -UPEDAL -DCAN3 -DUID_BASE --suppressions-list=tests/misra/suppressions.txt --dump --enable=all --inline-suppr --force board/main.c 2>/tmp/misra/cppcheck_output.txt || true
python tests/misra/cppcheck/addons/misra.py board/main.c.dump 2>/tmp/misra/misra_output.txt || true

# violations in safety files
(cat /tmp/misra/misra_output.txt | grep safety) > /tmp/misra/misra_safety_output.txt || true
(cat /tmp/misra/cppcheck_output.txt | grep safety) > /tmp/misra/cppcheck_safety_output.txt || true

if [[ -s "/tmp/misra/misra_safety_output.txt" ]] || [[ -s "/tmp/misra/cppcheck_safety_output.txt" ]]
then
  echo "Found Misra violations in the safety code:"
  cat /tmp/misra/misra_safety_output.txt
  cat /tmp/misra/cppcheck_safety_output.txt
  exit 1
fi

# pedal code
tests/misra/cppcheck/cppcheck -UPANDA -DPEDAL -UCAN3 --suppressions-list=tests/misra/suppressions.txt -I board/ --dump --enable=all --inline-suppr --force board/pedal/main.c 2>/tmp/misra/cppcheck_pedal_output.txt || true
python tests/misra/cppcheck/addons/misra.py board/pedal/main.c.dump 2>/tmp/misra/misra_pedal_output.txt || true
