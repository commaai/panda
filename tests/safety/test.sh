#!/usr/bin/env bash
set -e

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null && pwd)"
cd $DIR

HW_TYPES=( 6 9 )
for hw_type in "${HW_TYPES[@]}"; do
  echo "Testing HW_TYPE: $hw_type"
  HW_TYPE=$hw_type pytest -n auto test_*.py -k 'not Base'
done
