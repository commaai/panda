#!/usr/bin/env python3
import glob
import os
import shutil
import subprocess
import tempfile
import unittest

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
# only in ALLOW_DEBUG builds, see SConscript
DEBUG_SYMBOLS = {"self_test_inject", "relay_readback_inject", "harness_adc2_offset_mV", "stop_mode_requested"}
FLASH_SYMBOLS = {"flash_unlock", "flash_erase_sector", "flash_write_word"}


# names in the ELF string tables, so no binutils are needed
def symbols(elf):
  with open(elf, "rb") as f:
    dat = f.read()
  return {s for s in DEBUG_SYMBOLS | FLASH_SYMBOLS if b"\0" + s.encode() + b"\0" in dat}


class TestRelease(unittest.TestCase):
  @classmethod
  def setUpClass(cls):
    cls.tmp = tempfile.TemporaryDirectory()
    src = os.path.join(cls.tmp.name, "panda")
    shutil.copytree(ROOT, src, ignore=shutil.ignore_patterns(".git", ".venv", "obj"))
    os.makedirs(os.path.join(src, "board/obj"))
    env = {**os.environ, "RELEASE": "1", "CERT": os.path.join(src, "board/crypto/certs/debug")}
    subprocess.run(["scons", "-s"], cwd=src, env=env, check=True)
    cls.apps = glob.glob(os.path.join(src, "board/obj/*/main.elf"))
    cls.bootstubs = glob.glob(os.path.join(src, "board/obj/*/bootstub.elf"))

  @classmethod
  def tearDownClass(cls):
    cls.tmp.cleanup()

  def test_no_debug_features(self):
    self.assertTrue(self.apps)
    for elf in self.apps + self.bootstubs:
      self.assertFalse(DEBUG_SYMBOLS & symbols(elf), elf)

  def test_only_bootstub_programs_flash(self):
    for elf in self.apps:
      self.assertFalse(FLASH_SYMBOLS & symbols(elf), elf)
    for elf in self.bootstubs:
      self.assertTrue(FLASH_SYMBOLS & symbols(elf), elf)


if __name__ == "__main__":
  unittest.main()
