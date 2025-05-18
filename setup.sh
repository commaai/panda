#!/usr/bin/env bash
set -e

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null && pwd)"
cd $DIR

PLATFORM=$(uname -s)

echo "installing dependencies"
if [[ $PLATFORM == "Darwin" ]]; then
  brew install --cask gcc-arm-embedded
  brew install python3 dfu-util gcc@13
elif [[ $PLATFORM == "Linux" ]]; then
  sudo apt-get install -y --no-install-recommends \
    make g++ git libnewlib-arm-none-eabi \
    dfu-util gcc-arm-none-eabi python3-pip libffi-dev clang-17 python3
else
  echo "WARNING: unsupported platform. skipping apt/brew install."
fi

python3 -m venv .venv
source .venv/bin/activate

pip install -e .[dev]
