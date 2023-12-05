#!/usr/bin/env bash
set -e

HW_TYPES=( 6 9 )

for hw_type in "${HW_TYPES[@]}"; do
  echo "Testing HW_TYPE: $hw_type"
  HW_TYPE=$hw_type pytest -n auto --dist loadfile test_*.py -k 'not Base'
done
