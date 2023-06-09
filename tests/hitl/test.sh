#!/bin/bash
set -e

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null && pwd)"
cd $DIR

PYTHONUNBUFFERED=1 pytest --durations=0 --maxfail=1 *.py
