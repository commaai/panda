#!/usr/bin/env bash
set -e

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null && pwd)"
cd $DIR

scons -j$(nproc) -D

# run safety tests and generate coverage data
HW_TYPES=( 6 9 )
for hw_type in "${HW_TYPES[@]}"; do
  echo "Testing HW_TYPE: $hw_type"
  PYTHONPATH=/home/batman/:/home/batman/panda/opendbc/:$PYTHONPATH LLVM_PROFILE_FILE="safety_%m.profraw" HW_TYPE=$hw_type pytest test_*.py
done

# test line coverage
llvm-profdata-17 merge -sparse safety_*.profraw -o safety.profdata
INCOMPLETE_COVERAGE=$(llvm-cov-17 report -show-region-summary=false -show-branch-summary=false -instr-profile=safety.profdata ../libpanda/libpanda.so -sources ../../board/safety/safety_*.h | awk '$7 != "100.00%"' | head -n -1)
if [ ! $(echo "$INCOMPLETE_COVERAGE" | wc -l) -eq 2 ]; then
  echo "FAILED: Some files have less than 100% line coverage:"
  echo -e "$INCOMPLETE_COVERAGE"
  exit 1
else
  echo "SUCCESS: All checked files have 100% line coverage!"
fi
