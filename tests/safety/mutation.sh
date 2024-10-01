#!/usr/bin/env bash
set -e

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null && pwd)"
cd $DIR

MULL_OPS="mutators: [cxx_increment, cxx_decrement, cxx_comparison, cxx_bitwise_assignment, cxx_bitwise, cxx_arithmetic_assignment, cxx_arithmetic]"
MULL_MODE="${MULL_MODE:-SAFETY_ONLY}"

GIT_REF="${GIT_REF:-origin/master}"
GIT_ROOT=$(git rev-parse --show-toplevel)
MULL_GIT="gitDiffRef: $GIT_REF\ngitProjectRoot: $GIT_ROOT"

function write_conf() {
  echo -e "timeout: 10000\n$MULL_OPS\n$1" > $GIT_ROOT/mull.yml
}

$DIR/install_mull.sh

write_conf
scons --mutation -j$(nproc) -D

SAFETY_TESTS=$(find * | grep "^test_.*\.py")
for SAFETY_TEST in ${SAFETY_TESTS[@]}; do
  echo ""
  echo ""
  echo -e "Testing mutations on : $SAFETY_TEST"
  #if [[ $MULL_MODE == "SAFETY_ONLY" ]]; then
    #SAFETY_MODE=$(echo $SAFETY_TEST | sed -e 's/test_/safety_/g' | sed -e 's/\.py/\.h/g')
    #write_conf "includePaths:\n - $SAFETY_MODE"
  #else
    #write_conf "$MULL_GIT"
  #fi
  mull-runner-17 --ld-search-path /lib/x86_64-linux-gnu/ ../libpanda/libpanda.so -test-program=./$SAFETY_TEST > "FAILURES_$SAFETY_TEST" || true
done
