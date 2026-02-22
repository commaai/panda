#!/usr/bin/env bash
set -e

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null && pwd)"
cd $DIR

PLATFORM=$(uname -s)

echo "installing dependencies"
if [[ $PLATFORM == "Darwin" ]]; then
  # pass
  :
elif [[ $PLATFORM == "Linux" ]]; then
  # for AGNOS since we clear the apt lists
  if [[ ! -d /"var/lib/apt/" ]]; then
    sudo apt update
  fi

  sudo apt-get install -y --no-install-recommends \
    curl ca-certificates \
    make g++ git \
    libusb-1.0-0 \
    python3-dev python3-pip python3-venv
else
  echo "WARNING: unsupported platform. skipping apt/brew install."
fi

if ! command -v uv &>/dev/null; then
  echo "'uv' is not installed. Installing 'uv'..."
  curl -LsSf https://astral.sh/uv/install.sh | sh

  # doesn't require sourcing on all platforms
  set +e
  source $HOME/.local/bin/env
  set -e
fi

export UV_PROJECT_ENVIRONMENT="$DIR/.venv"
uv sync --all-extras --upgrade
source "$DIR/.venv/bin/activate"

# extract vendored toolchain
if [[ "$OSTYPE" == darwin* ]]; then
  ARCHNAME="Darwin"
elif [[ "$(uname -m)" == "aarch64" ]]; then
  ARCHNAME="aarch64"
else
  ARCHNAME="x86_64"
fi
if [ ! -d "$DIR/.bin" ]; then
  echo "Extracting gcc-arm-none-eabi toolchain for $ARCHNAME..."
  tar xf "$DIR/gcc-arm-none-eabi.tar.gz" -C "$DIR" "$ARCHNAME/"
  mv "$DIR/$ARCHNAME" "$DIR/.bin"
fi
export PATH="$DIR/.bin/bin:$PATH"
