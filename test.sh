#!/usr/bin/env bash
set -e

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null && pwd)"
cd $DIR

# *** env setup ***
source ./setup.sh

# *** build ***
scons -j8

# *** lint ***
if [[ "${GITHUB_ACTIONS:-}" == "true" ]]; then
  ruff check --format=github .
else
  ruff check .
fi
mypy python/


# *** test ***

# TODO: make randomly work
pytest --randomly-dont-reorganize tests/
