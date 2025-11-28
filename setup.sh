#!/usr/bin/env bash
set -e

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null && pwd)"
cd $DIR

PLATFORM=$(uname -s)

echo "installing dependencies"

export UV_PROJECT_ENVIRONMENT="$DIR/.venv"
# uv sync --all-extras --upgrade
source "$DIR/.venv/bin/activate"
