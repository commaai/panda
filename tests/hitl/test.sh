#!/bin/bash
set -e

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null && pwd)"
cd $DIR


# n = number of pandas tested
NO_JUNGLE=1 pytest --durations=0 test.py -n 5 --dist loadgroup -v -s

#pytest --durations=0 --maxfail=1 *.py
