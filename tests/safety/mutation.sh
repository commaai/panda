#!/usr/bin/env bash
set -e

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null && pwd)"
cd $DIR

cd ../../
scons --mutation -j$(nproc)
cd $DIR

GIT_REF="${GIT_REF:-origin/master}"

SAFETY_MODELS=$(find * | grep "^test_.*\.py")
for safety_model in ${SAFETY_MODELS[@]}; do
  echo ""
  echo ""
  echo -e "Testing mutations on : $safety_model"
  echo -e "gitDiffRef: $GIT_REF\ngitProjectRoot: ../../" > mull.yml

  #PYTHONPATH=/home/batman/:/home/batman/panda/opendbc/:$PYTHONPATH mull-runner-17 --ld-search-path /lib/x86_64-linux-gnu/ ../libpanda/libpanda.so -test-program=./$safety_model
  mull-runner-17 --ld-search-path /lib/x86_64-linux-gnu/ ../libpanda/libpanda.so -test-program=./$safety_model
done
