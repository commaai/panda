#!/usr/bin/env bash
set -e

# reset coverage data and generate gcc note file
rm -f ../libpanda/*.gcda
scons -j$(nproc) -D --coverage

HW_TYPES=( 6 7 )
for hw_type in "${HW_TYPES[@]}"; do
  echo "Testing HW_TYPE: $hw_type"
  HW_TYPE=$hw_type python -m unittest discover .
done
#./test_honda.py

# generate and open report
if [ "$1" == "--report" ]; then
  geninfo ../libpanda/ -o coverage.info
  genhtml coverage.info -o coverage-out
  browse coverage-out/index.html
fi

# test coverage
GCOV_OUTPUT=$(gcov -n ../libpanda/panda.c)
INCOMPLETE_COVERAGE=$(echo "$GCOV_OUTPUT" | paste -s -d' \n' | grep "File.*safety/safety_.*.h" | grep -v "100.00%" || true)
if [ -n "$INCOMPLETE_COVERAGE" ]; then
  echo "FAILED: Some files have less than 100% coverage:"
  echo "$INCOMPLETE_COVERAGE"
  exit 1
else
  echo "SUCCESS: All checked files have 100% coverage!"
fi
