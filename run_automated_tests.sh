#!/bin/bash
PYTHONPATH="." nosetests -v --with-xunit -s tests/automated/$1*.py
