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
uvx ty check python/ --ignore unresolved-attribute --ignore unresolved-import


# *** test ***

# TODO: make xdist and randomly work
pytest -n0 --randomly-dont-reorganize tests/
