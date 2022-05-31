#!/bin/sh
#
# Patched this script for Oil

set -o errexit
set -o nounset

rm -f -v yajl.so
python2 setup.py build_ext --inplace

PYTHONPATH=. python tests/unit.py 

zcat test_data/issue_11.gz | PYTHONPATH=. ./tests/issue_11.py 

# Not supporting Python 3
return

python3 setup.py build && PYTHONPATH=build/lib.linux-x86_64-3.1 python3 tests/unit.py
