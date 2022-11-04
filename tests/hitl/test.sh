#!/bin/bash
set -e

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null && pwd)"
nosetests -x -v --with-flaky -s $(ls $DIR/$1*.py)
