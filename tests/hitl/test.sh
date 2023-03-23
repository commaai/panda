#!/bin/bash
set -e

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null && pwd)"
cd $DIR

#pytest -v -s --durations=0 *.py
#pytest -v -s --durations=0 5_gps.py
pytest -v -s --durations=0 *.py
