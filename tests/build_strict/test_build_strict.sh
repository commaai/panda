#!/bin/bash -e

cd ../../board/

make -f Makefile.strict clean
make -f Makefile.strict bin 2> compiler_output.txt

if [[ -s "compiler_output.txt" ]]
then
  echo "Found alerts from the compiler:"
  cat compiler_output.txt
  exit 1
fi

cd pedal/

make clean
make bin 2> compiler_pedal_output.txt

if [[ -s "compiler_pedal_output.txt" ]]
then
  echo "Found alerts from the compiler:"
  cat compiler_pedal_output.txt
  exit 1
fi
