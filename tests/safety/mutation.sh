#!/usr/bin/env bash
set -e

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null && pwd)"
cd $DIR

# TODO: add more ops from https://mull.readthedocs.io/en/latest/SupportedMutations.html
# Note that cxx_assign_const and cxx_init_const are currently broken
MUTATION_OPS="mutators: [cxx_increment, cxx_decrement, cxx_comparison, cxx_boundary, cxx_bitwise_assignment, cxx_bitwise, cxx_arithmetic_assignment, cxx_arithmetic]"

# TODO: add more files from board/safety
MUTATION_SAFETY_FILES=( safety_body.h safety_defaults.h safety_elm327.h )

# SAFETY_ONLY -> verify mutations on safety_xx.h with test_xx.py
# DIFF_COVERAGE -> verify mutations with test_xx.py on the intersection between its code coverage and the current git diff
MUTATION_MODE="${MUTATION_MODE:-DIFF_COVERAGE}"

GIT_REF="${GIT_REF:-origin/master}"
GIT_ROOT=$(git rev-parse --show-toplevel)
GIT_MULL_CONFIG="gitDiffRef: $GIT_REF\ngitProjectRoot: $GIT_ROOT"

function write_conf() {
  echo -e "timeout: 10000\n$MUTATION_OPS\n$1" > $GIT_ROOT/mull.yml
}

$DIR/install_mull.sh

write_conf
scons --mutation -j$(nproc) -D

SAFETY_TESTS=$(find * | grep "^test_.*\.py")
for SAFETY_TEST in ${SAFETY_TESTS[@]}; do
  if [[ $MUTATION_MODE == "SAFETY_ONLY" ]]; then
    SAFETY_MODE=$(echo $SAFETY_TEST | sed -e 's/test_/safety_/g' | sed -e 's/\.py/\.h/g')
    if [[ ! " ${MUTATION_SAFETY_FILES[*]} " =~ [[:space:]]${SAFETY_MODE}[[:space:]] ]]; then
      continue
    fi
    write_conf "includePaths:\n - $SAFETY_MODE"
  else
    write_conf "$GIT_MULL_CONFIG"
  fi
  echo ""
  echo ""
  echo -e "Testing mutations on : $SAFETY_TEST"
  mull-runner-17 --ld-search-path /lib/x86_64-linux-gnu/ ../libpanda/libpanda.so -test-program=./$SAFETY_TEST
done
