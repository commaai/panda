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
  # default
  (None, None, False),
  # misra-c2012-10.4
  ("board/main.c", "1i int test(int tmp, float tmp2) { return tmp - tmp2; }", True),
  # misra-c2012-15.5
  ("board/main.c", "1i bool test(bool state){ if (state) { return true; } else { return false; } }", True),
  # misra-c2012-12.1
  ("board/main.c", "1i int test(int tmp) { return tmp == 8 ? 1 : 2; }", True),
  # misra-c2012-13.3
  ("board/main.c", "1i void test(int tmp) { int tmp2 = tmp++ + 2; if (tmp2) {;}}", True),
  # misra-c2012-13.4
  ("board/main.c", "1i int test(int x, int y) { return (x=2) && (y=2); }", True),
  # misra-c2012-13.5
  ("board/main.c", "1i void test(int tmp) { if (true && tmp++) {;} }", True),
  # misra-c2012-13.6
  ("board/main.c", "1i void test(int tmp) { if (sizeof(tmp++)) {;} }", True),
  # misra-c2012-14.1
  ("board/main.c", "1i void test(float len) { for (float j = 0; j < len; j++) {;} }",True),
  # misra-c2012-14.4
  ("board/main.c", "1i void test(int len) { if (len - 8) {;} }", True),
  # misra-c2012-16.4
  ("board/main.c", r"1i void test(int temp) {switch (temp) { case 1: ; }}\n", True),
  # misra-c2012-17.8
  ("board/main.c", "1i void test(int cnt) { for (cnt=0;;cnt++) {;} }", True),
  # misra-c2012-20.4
  ("board/main.c", r"1i #define auto 1\n", True),
  # misra-c2012-20.5
  ("board/main.c", r"1i #define TEST 1\n#undef TEST\n", True),
]

@pytest.mark.parametrize("fn, patch, should_fail", mutations)
def test_misra_mutation(fn, patch, should_fail):
  key = hashlib.md5((str(fn) + str(patch)).encode()).hexdigest()
  tmp = os.path.join(tempfile.gettempdir(), key)

  del_header = "/[#include, #ifdef, #else]/d" if fn is not None else None

  if os.path.exists(tmp):
    shutil.rmtree(tmp)
  shutil.copytree(ROOT, tmp)

  # apply patch
  if fn is not None:
    r = os.system(f"cd {tmp} && sed -i -e '{patch}' -e '{del_header}' {fn}")
    assert r == 0

  # run test
  r = subprocess.run("tests/misra/test_misra.sh", cwd=tmp, shell=True)
  failed = r.returncode != 0
  assert failed == should_fail

  shutil.rmtree(tmp)

if __name__ == "__main__":
  pytest.main([__file__, "-n 8"])
