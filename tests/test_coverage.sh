#!/usr/bin/env bash
set -e

# reset coverage data and generate gcc note file
rm -f libpanda/*.gcda
rm -rf libpanda/coverage-out
rm -f libpanda/coverage.info
scons -j8 -D --safety-coverage

# run safety tests to generate coverage data
./safety/test.sh

# generate and open report
geninfo libpanda/ -o libpanda/coverage.info
genhtml libpanda/coverage.info -o libpanda/coverage-out
browse libpanda/coverage-out/index.html
