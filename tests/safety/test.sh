#!/usr/bin/env bash

# Loop over all HW_TYPEs, see board/boards/board_declarations.h
# Make sure test fails if one HW_TYPE fails
set -e

HW_TYPES=( 6 7 9 )
for hw_type in "${HW_TYPES[@]}"; do
  echo "Testing HW_TYPE: $hw_type"
  HW_TYPE=$hw_type python -m unittest discover .
done
