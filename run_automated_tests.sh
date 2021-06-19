#!/bin/bash -e
TEST_SCRIPTS=$(ls tests/automated/$1*.py)
PYTHONPATH="." $(which nosetests) -v -s $TEST_SCRIPTS
