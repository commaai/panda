#!/bin/bash
TEST_FILENAME=${TEST_FILENAME:-nosetests.xml}
PYTHONPATH="." nosetests -v --with-xunit --xunit-file=./$TEST_FILENAME -s tests/automated/$1*.py
