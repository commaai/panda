#!/usr/bin/env bash
set -e

# reset coverage data and generate gcc note file
scons -j$(nproc) -D --coverage

# run safety tests to generate coverage data
./test.sh

# generate and open report
geninfo ../libpanda/ -o coverage.info
genhtml coverage.info -o coverage-out
browse coverage-out/index.html
