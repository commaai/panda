#!/usr/bin/env python3
import os
import pytest
import shutil
import subprocess
import tempfile
import hashlib

HERE = os.path.abspath(os.path.dirname(__file__))
ROOT = os.path.join(HERE, "../../")

# TODO: test more cases
# - at least one violation in each safety/safety*.h file
# - come up with a pattern for each rule (cppcheck tests probably have good ones?)
mutations = [
  (None, None, False),
  # F4 only
  ("board/stm32fx/llbxcan.h", "s/1U/1/g", True),
  # H7 only
  ("board/stm32h7/llfdcan.h", "s/return ret;/if (true) { return ret; } else { return false; }/g", True),
  # general safety
  ("board/safety/safety_toyota.h", "s/is_lkas_msg =.*;/is_lkas_msg = addr == 1 || addr == 2;/g", True),
]

@pytest.mark.parametrize("fn, patch, should_fail", mutations)
def test_misra_mutation(fn, patch, should_fail):
  key = hashlib.md5((str(fn) + str(patch)).encode()).hexdigest()
  tmp = os.path.join(tempfile.gettempdir(), key)

  if os.path.exists(tmp):
    shutil.rmtree(tmp)
  shutil.copytree(ROOT, tmp)

  # apply patch
  if fn is not None:
    r = os.system(f"cd {tmp} && sed -i '{patch}' {fn}")
    assert r == 0

  # run test
  r = subprocess.run("tests/misra/test_misra.sh", cwd=tmp, shell=True)
  failed = r.returncode != 0
  assert failed == should_fail

  shutil.rmtree(tmp)

if __name__ == "__main__":
  pytest.main([__file__, "-n 4"])
