#!/usr/bin/env python3
import os
import glob
import re
import unittest
import shutil
import subprocess
import tempfile
import random
from concurrent.futures import ProcessPoolExecutor

HERE = os.path.abspath(os.path.dirname(__file__))
ROOT = os.path.join(HERE, "../../")

# skip mutating these paths
IGNORED_PATHS = (
  'board/obj',
  'board/body',
  'board/stm32h7/inc',
  'board/fake_stm.h',

  # bootstub only files
  'board/flasher.h',
  'board/bootstub.c',
  'board/bootstub_declarations.h',
  'board/stm32h7/llflash.h',
  'board/crypto',
)

mutations = [
  # no mutation, should pass
  (None, None, None, False),
  ("board/stm32h7/llfdcan.h", "misra-c2012-15.5", lambda s: s.replace("return ret;", "if (true) { return ret; } else { return false; }"), True),
]

patterns = [
  ("misra-c2012-13.3", lambda s: s + "\nvoid test(int tmp) { int tmp2 = tmp++ + 2; if (tmp2) {;}}\n"),
  ("misra-c2012-13.4", lambda s: s + "\nint test(int x, int y) { return (x=2) && (y=2); }\n"),
  ("misra-c2012-13.5", lambda s: s + "\nvoid test(int tmp) { if (true && tmp++) {;} }\n"),
  ("misra-c2012-13.6", lambda s: s + "\nvoid test(int tmp) { if (sizeof(tmp++)) {;} }\n"),
  ("misra-c2012-14.1", lambda s: s + "\nvoid test(float len) { for (float j = 0; j < len; j++) {;} }\n"),
  ("misra-c2012-14.4", lambda s: s + "\nvoid test(int len) { if (len - 8) {;} }\n"),
  ("misra-c2012-16.4", lambda s: s + "\nvoid test(int temp) {switch (temp) { case 1: ; }}\n"),
  ("misra-c2012-17.8", lambda s: s + "\nvoid test(int cnt) { for (cnt=0;;cnt++) {;} }\n"),
  ("misra-c2012-20.4", lambda s: s + "\n#define auto 1\n"),
  ("misra-c2012-20.5", lambda s: s + "\n#define TEST 1\n#undef TEST\n"),
]

all_files = glob.glob('board/**', root_dir=ROOT, recursive=True)
files = sorted(f for f in all_files if f.endswith(('.c', '.h')) and not f.startswith(IGNORED_PATHS))
assert len(files) > 50, all(d in files for d in ('board/main.c', 'board/stm32h7/llfdcan.h'))

# fixed seed for reproducible mutation selection
rng = random.Random(len(files))
for p in patterns:
  mutations.append((rng.choice(files), *p, True))

# sample to keep CI fast, but always include the no-mutation case
mutations = [mutations[0]] + rng.sample(mutations[1:], min(2, len(mutations) - 1))


def apply_mutation(content, transform):
  # keep the added code inside the include guard, so it is only added once
  m = re.search(r"^#endif  // BOARD_\w+_H\n\Z", content, re.MULTILINE)
  if m is None:
    return transform(content)
  return transform(content[:m.start()]) + "\n" + content[m.start():]


def run_mutation(index):
  # mutations hold lambdas, which can't be pickled, so workers look theirs up by index
  fn, _, transform, _ = mutations[index]
  with tempfile.TemporaryDirectory() as tmp:
    shutil.copytree(ROOT, tmp + "/panda", ignore=shutil.ignore_patterns(".git", ".venv", "__pycache__"))

    # apply patch
    if fn is not None:
      fpath = os.path.join(tmp, "panda", fn)
      with open(fpath) as f:
        content = f.read()
      with open(fpath, "w") as f:
        f.write(apply_mutation(content, transform))

    return subprocess.run("SKIP_BUILD=1 panda/tests/misra/test_misra.sh", cwd=tmp, shell=True,
                          stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)


class TestMisraMutation(unittest.TestCase):
  @classmethod
  def setUpClass(cls):
    # Each mutation has its own checkout and subprocess; keep the expensive checks parallel.
    executor = ProcessPoolExecutor(max_workers=len(mutations))
    cls.addClassCleanup(executor.shutdown)
    cls.results = [executor.submit(run_mutation, i) for i in range(len(mutations))]


def mutation_test(index):
  def test(self):
    result = self.results[index].result()
    _, rule, _, should_fail = mutations[index]
    self.assertEqual(result.returncode != 0, should_fail, result.stdout)
    if should_fail:
      self.assertIn(rule, result.stdout, "MISRA test failed but not for the correct violation")
  test.__doc__ = f"MISRA mutation: {mutations[index][:2]}"
  return test


for index in range(len(mutations)):
  setattr(TestMisraMutation, f"test_misra_mutation_{index}", mutation_test(index))
