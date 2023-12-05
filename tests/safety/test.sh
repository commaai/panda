#!/usr/bin/env bash
set -e

HW_TYPES=( 6 )

for hw_type in "${HW_TYPES[@]}"; do
  echo "Testing HW_TYPE: $hw_type"
  #HW_TYPE=$hw_type python -m unittest discover .
  HW_TYPE=$hw_type pytest -n auto --dist loadfile .
done
