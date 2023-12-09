#!/usr/bin/env python3
import os
import glob
import pytest
import subprocess

HERE = os.path.abspath(os.path.dirname(__file__))
ROOT = os.path.join(HERE, "../../")

# TODO: F4 only, H7 only, and pedal only

mutations = [
  ("safety_toyota.h", ""),
]

def run_misra():
  r = subprocess.run("./test_misra.sh", cwd=HERE, shell=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
  return r.returncode == 0

def test_misra_mutation():
  # clean, should pass
  assert run_misra()

  # F4 change

  # H7 only change

  # pedal only change

  # safety change


if __name__ == "__main__":
  pytest.main()
