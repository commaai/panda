#!/usr/bin/env bash
set -e

rm -f libpanda/*.gcda
rm -f libpanda/*.gcov
rm -f *.gcov
rm -rf libpanda/coverage-out
scons -j8 -D --safety-coverage


./safety/test.sh

gcov libpanda/panda.c
lcov --capture --directory libpanda/ --output-file libpanda/coverage.info
genhtml libpanda/coverage.info --output-directory libpanda/coverage-out
