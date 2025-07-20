#!/usr/bin/env bash
set -e

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null && pwd)"
cd $DIR

# *** env setup ***
source ./setup.sh

# *** build ***
scons -j8

# *** lint ***
ruff check .
#mypy python/

# *** test ***

tests/misra/test_misra.sh

# TODO: make randomly work
pytest --randomly-dont-reorganize tests/
