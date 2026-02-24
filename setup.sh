#!/usr/bin/env bash
set -e

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null && pwd)"
cd $DIR

# libusb is needed at runtime by python-libusb1
if [ -f /etc/debian_version ]; then
  SUDO=$(if [ "$(id -u)" -ne 0 ]; then echo "sudo"; fi)
  $SUDO apt-get update && $SUDO apt-get install -y --no-install-recommends libusb-1.0-0
elif [ -f /etc/alpine-release ]; then
  apk add --no-cache libusb
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
