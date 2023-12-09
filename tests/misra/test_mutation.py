#!/usr/bin/env python3
import os
import glob
import pytest
import subprocess

HERE = os.path.abspath(os.path.dirname(__file__))
ROOT = os.path.join(HERE, "../../")

# TODO: test more cases (e.g. one violation in each safety*.h) and all rules
mutations = [
  # F4 only
  ("board/stm32fx/bxcan.h", "s/1U/1/g"),
  # H7 only
  ("board/stm32h7/", "s/return ret;/if (true) { return ret; } else { return false; }/g"),
  # general safety
  ("board/safety/safety_toyota.h", "s/is_lkas_msg =.*;/is_lkas_msg = addr == 1 || addr == 2;/g"),
]

def patch(fn, pt):
  r = os.system(f"cd {ROOT} && git checkout . && sed -i '{pt}' {fn}")
  assert r == 0

def run_misra():
  r = subprocess.run("./test_misra.sh", cwd=HERE, shell=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
  return r.returncode == 0

def test_misra_mutation():
  #assert run_misra()
  for fn, pt in mutations:
    print(f"file: {fn}, patch: {pt}")
    patch(fn, pt)
    assert not run_misra()

if __name__ == "__main__":
  pytest.main()
