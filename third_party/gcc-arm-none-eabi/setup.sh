#!/usr/bin/env bash
set -e

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null && pwd)"

# detect platform
if [[ "$OSTYPE" == darwin* ]]; then
  ARCHNAME="Darwin"
elif [[ "$(uname -m)" == "aarch64" ]]; then
  ARCHNAME="aarch64"
else
  ARCHNAME="x86_64"
fi

# extract if not already present
if [ ! -d "$DIR/$ARCHNAME" ]; then
  echo "Extracting gcc-arm-none-eabi toolchain for $ARCHNAME..."
  tar xf "$DIR/gcc-arm-none-eabi.tar.gz" -C "$DIR" "$ARCHNAME/"
fi
