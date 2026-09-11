#!/usr/bin/env bash
set -e

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null && pwd)"
export CERT=/home/batman/xx/pandaextra/certs/release

if [ ! -f "$CERT" ]; then
  echo "No release cert found, cannot build release."
  echo "You probably aren't looking to do this anyway."
  exit
fi

export RELEASE=1
export BUILDER=DEV

cd "$DIR/.."
make clean
make -j4 firmware
cd board/obj
RELEASE_NAME=$(awk '{print $1}' version)
mkdir -p ../../release
zip -j ../../release/panda-$RELEASE_NAME.zip version panda_h7.bin.signed bootstub.panda_h7.bin
