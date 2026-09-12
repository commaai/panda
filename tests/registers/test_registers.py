import os
from pathlib import Path
import subprocess
import tempfile
import unittest

import opendbc


class TestRegisters(unittest.TestCase):
  def test_register_tracking(self):
    root = Path(__file__).resolve().parents[2]
    with tempfile.TemporaryDirectory() as tmp:
      executable = Path(tmp) / "test_registers"
      subprocess.run([
        os.environ.get("CC", "cc"), "-std=gnu11", "-Wall", "-Wextra", "-Werror",
        "-Wno-pointer-to-int-cast", "-Wno-int-to-pointer-cast", "-DSTM32H725xx",
        "-I", opendbc.INCLUDE_PATH, "-I", str(root), "-I", str(root / "board/stm32h7/inc"),
        str(Path(__file__).with_suffix(".c")), "-o", str(executable),
      ], check=True)
      subprocess.run([str(executable)], check=True)
      self.assertEqual(subprocess.run([str(executable), "unknown"], check=False).returncode, 23)


if __name__ == "__main__":
  unittest.main()
