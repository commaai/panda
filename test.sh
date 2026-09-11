#!/usr/bin/env bash
set -e

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null && pwd)"
cd $DIR

# *** env setup ***
source ./setup.sh

# *** build ***
scons

# *** lint + test ***
ruff check .
python -W error -m unittest discover -s tests/usbprotocol -v
python -W error -m unittest discover -s tests/misra -v
