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
  ("board/stm32fx/llbxcan.h", "$ a int test(int tmp, float tmp2) { return tmp - tmp2; }", True),
  # H7 only
  ("board/stm32h7/llfdcan.h", "$ a bool test(bool state){ if (state) { return true; } else {return false; } }", True),
  # general safety
  ("board/safety/safety_toyota.h", "s/is_lkas_msg =.*;/is_lkas_msg = addr == 1 || addr == 2;/g", True),
  # misra-c2012-12.1
  ("board/safety/safety_chrysler.h", "$ a bool test(int tmp) { return tmp == 8 ? true : false; }", True),
  # misra-c2012-13.3
  ("board/safety/safety_elm327.h", "$ a void test(int tmp) { int tmp2 = tmp++ + 2; if (tmp2) {;}}", True),
  # misra-c2012-13.4
  ("board/safety/safety_defaults.h", "$ a int test(int x, int y) { return (x=2) && (y=2); }", True),
  # misra-c2012-13.5
  ("board/safety/safety_defaults.h", "$ a void test(int tmp) { if (true && tmp++) {;} }", True),
  # misra-c2012-13.6
  ("board/safety/safety_elm327.h", "$ a void test(int tmp) { if (sizeof(tmp++)) {;} }", True),
  # misra-c2012-14.1
  ("board/safety/safety_elm327.h", "$ a void test(float len) { for (float j = 0; j < len; j++) {;} }",True),
  # misra-c2012-14.4
  ("board/safety/safety_elm327.h", "$ a void test(int len) { if (len - 8) {;} }", True),
  # misra-c2012-16.4
  ( "board/safety/safety_elm327.h", r"$ a void test(int temp) {switch (temp) { case 1: ; }}\n", True),
  # misra-c2012-20.4
  ( "board/safety/safety_elm327.h", r"$ a #define auto 1\n", True),
  # misra-c2012-20.5
  ( "board/safety/safety_elm327.h", r"$ a #define TEST 1\n#undef TEST\n", True),
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
  pytest.main([__file__, "-n 8"])
