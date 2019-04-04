#!/bin/bash
PYTHONPATH="." nosetests -v -x -s tests/automated/$1*.py
