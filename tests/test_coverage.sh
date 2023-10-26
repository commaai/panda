#!/usr/bin/env bash
set -e

# reset coverage data and generate gcc note file
rm -f libpanda/*.gcda
rm -rf libpanda/coverage-out
rm -f libpanda/coverage.info
scons -j$(nproc) -D --safety-coverage

# run safety tests to generate coverage data
./safety/test.sh

# generate and open report
#geninfo libpanda/ -o libpanda/coverage.info
#genhtml libpanda/coverage.info -o libpanda/coverage-out
#browse libpanda/coverage-out/index.html

# test coverage
GCOV_OUTPUT=$(gcov -n libpanda/panda.c)
INCOMPLETE_COVERAGE=$(echo "$GCOV_OUTPUT" | paste -s -d' \n' | grep "File.*safety/safety_.*.h" | grep -v "100.00%")
if [ ! -z "$INCOMPLETE_COVERAGE" ]; then
  echo "Some files have less than 100% coverage:"
  echo "$INCOMPLETE_COVERAGE"
  exit 1
else
  echo "All checked files have 100% coverage!"
fi
