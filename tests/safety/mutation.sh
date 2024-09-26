#!/usr/bin/env bash
set -e

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null && pwd)"
cd $DIR

$DIR/install_mull.sh

scons --mutation -j$(nproc) -D

GIT_REF="${GIT_REF:-origin/master}"
echo -e "mutators:\n  - cxx_all\ntimeout: 10000\ngitDiffRef: $GIT_REF\ngitProjectRoot: ../../" > mull.yml

SAFETY_MODELS=$(find * | grep "^test_.*\.py")
for safety_model in ${SAFETY_MODELS[@]}; do
  echo ""
  echo ""
  echo -e "Testing mutations on : $safety_model"
  mull-runner-17 --ld-search-path /lib/x86_64-linux-gnu/ ../libpanda/libpanda.so -test-program=./$safety_model
done
