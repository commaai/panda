#!/bin/bash
set -e

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null && pwd)"
cd $DIR

rm test_*.py
git checkout test_nissan.py
rm -f .mutmut-cache
mutmut run --runner="./test_nissan.py" --rerun-all --disable-mutation-types=string --paths-to-mutate $DIR --tests-dir $DIR/
