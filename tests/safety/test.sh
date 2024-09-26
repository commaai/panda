#!/usr/bin/env bash
set -e

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null && pwd)"
cd $DIR

rm -f safety_*.profraw safety.profdata
scons -j$(nproc) -D

# run safety tests and generate coverage data
HW_TYPES=( 6 9 )
for hw_type in "${HW_TYPES[@]}"; do
  echo "Testing HW_TYPE: $hw_type"
  LLVM_PROFILE_FILE="safety_%m.profraw" HW_TYPE=$hw_type pytest test_*.py
done

# generate coverage report
llvm-profdata-17 merge -sparse safety_*.profraw -o safety.profdata

# open html report
if [ "$1" == "--report" ]; then
  llvm-cov-17 show -format=html -show-branches=count -instr-profile=safety.profdata ../libpanda/libpanda.so -sources ../../board/safety/safety_*.h ../../board/safety.h -o coverage_report
  sensible-browser coverage_report/index.html
fi

# test line coverage
INCOMPLETE_COVERAGE=$(llvm-cov-17 report -show-region-summary=false -show-branch-summary=false -instr-profile=safety.profdata ../libpanda/libpanda.so -sources ../../board/safety/safety_*.h ../../board/safety.h | awk '$7 != "100.00%"' | head -n -1)
if [ ! $(echo "$INCOMPLETE_COVERAGE" | wc -l) -eq 2 ]; then
  echo "FAILED: Some files have less than 100% line coverage:"
  echo "$INCOMPLETE_COVERAGE"
  llvm-cov-17 show -line-coverage-lt=100 -instr-profile=safety.profdata ../libpanda/libpanda.so -sources ../../board/safety/safety_*.h ../../board/safety.h
  exit 1
else
  echo "SUCCESS: All checked files have 100% line coverage!"
fi
