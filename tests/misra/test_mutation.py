#!/usr/bin/env python3
import os
import pytest
import shutil
import subprocess
import tempfile

HERE = os.path.abspath(os.path.dirname(__file__))
ROOT = os.path.join(HERE, "../../")

# TODO: test more cases
# - at least one violation in each safety/safety*.h file
# - come up with a pattern for each rule (cppcheck tests probably have good ones?)
mutations = [
  # F4 only
  ("board/stm32fx/llbxcan.h", "s/1U/1/g", True),
  # H7 only
  ("board/stm32h7/llfdcan.h", "s/return ret;/if (true) { return ret; } else { return false; }/g", True),
  # general safety
  ("board/safety/safety_toyota.h", "s/is_lkas_msg =.*;/is_lkas_msg = addr == 1 || addr == 2;/g", True),
  # misra-c2012-12.1
  ("board/safety/safety_chrysler.h", "s/(chrysler_platform == CHRYSLER_PACIFICA)/chrysler_platform == CHRYSLER_PACIFICA/g", True),
  # misra-c2012-13.3
  ("board/safety/safety_defaults.h", "s/bus_fwd = 2;/int temp = 0;temp = bus_fwd++ + 2;bus_fwd = temp;/g", True),
  # misra-c2012-13.4
  ("board/safety/safety_defaults.h", "s/bus_fwd = 2;/int x; int y; bus_fwd = (x=2) && (y=2);/g", True),
  # misra-c2012-13.5
  ("board/safety/safety_defaults.h", "s/bus_fwd = 2;/int temp = 0; if (true && temp++) { bus_fwd = 2; }/g", True),
  # misra-c2012-13.6
  ("board/safety/safety_defaults.h", "s/bus_fwd = 2;/int temp = 0; if (sizeof(temp++)) { bus_fwd = 2; }/g", True),
  # misra-c2012-14.1
  ("board/safety/safety_elm327.h", "$ a for (float j = 0; j < (float)1; j++) {continue)",True),
  # misra-c2012-14.4
  ("board/safety/safety_elm327.h", "$ a int len = 10; if (len - 8) {;}", True),
  # misra-c2012-16.4
  ( "board/safety/safety_elm327.h", r"$ a void test(int temp) {switch (temp) { case 1: ; }}\n", True),
  # misra-c2012-20.4
  ( "board/safety/safety_elm327.h", r"$ a #define auto 1\n", True),
  # misra-c2012-20.5
  ( "board/safety/safety_elm327.h", r"$ a #define TEST 1\n#undef TEST\n", True),
]

@pytest.mark.parametrize("fn, patch, should_fail", mutations)
def test_misra_mutation(fn, patch, should_fail):
  with tempfile.TemporaryDirectory() as tmp:
    shutil.copytree(ROOT, tmp, dirs_exist_ok=True)

    # apply patch
    if fn is not None:
      r = os.system(f"cd {tmp} && sed -i '{patch}' {fn}")
      assert r == 0

    # run test
    r = subprocess.run("tests/misra/test_misra.sh", cwd=tmp, shell=True,
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    failed = r.returncode != 0
    assert failed == should_fail

if __name__ == "__main__":
  pytest.main([__file__, "-n 8"])
