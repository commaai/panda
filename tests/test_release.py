#!/usr/bin/env python3
import glob
import os
import shutil
import struct
import subprocess
import tempfile
import unittest

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
# only in ALLOW_DEBUG builds, see SConscript
DEBUG_SYMBOLS = {"self_test_inject", "relay_readback_inject", "harness_adc2_offset_mV", "stop_mode_requested"}
# linked in every build, only used with ALLOW_DEBUG
DEBUG_REFS = {"alloutput_hooks", "debug_rsa_key"}
FLASH_SYMBOLS = {"flash_unlock", "flash_erase_sector", "flash_write_word"}


# parses the ELF in python, the toolchain package has no binutils
def read_elf(elf):
  with open(elf, "rb") as f:
    dat = f.read()
  shoff, = struct.unpack_from("<I", dat, 0x20)
  shentsize, shnum = struct.unpack_from("<HH", dat, 0x2e)
  shdrs = [struct.unpack_from("<10I", dat, shoff + i * shentsize) for i in range(shnum)]
  # PROGBITS sections that are flashed
  image = b"".join(dat[s[4]:s[4] + s[5]] for s in shdrs if s[1] == 1 and s[2] & 2)
  syms = {}
  for s in shdrs:
    if s[1] == 2:
      strtab = shdrs[s[6]][4]
      for off in range(s[4], s[4] + s[5], 16):
        name, value = struct.unpack_from("<II", dat, off)
        syms[dat[strtab + name:dat.index(b"\0", strtab + name)].decode()] = value
  return syms, image


def symbols(elf):
  syms, image = read_elf(elf)
  refs = {s for s in DEBUG_REFS if s in syms and struct.pack("<I", syms[s]) in image}
  return ((DEBUG_SYMBOLS | FLASH_SYMBOLS) & syms.keys()) | refs


def build(tmp, release):
  src = os.path.join(tmp, "release" if release else "debug")
  shutil.copytree(ROOT, src, ignore=shutil.ignore_patterns(".git", ".venv", "obj"))
  os.makedirs(os.path.join(src, "board/obj"))
  env = dict(os.environ)
  if release:
    env |= {"RELEASE": "1", "CERT": os.path.join(src, "board/crypto/certs/debug")}
  subprocess.run(["scons", "-s"], cwd=src, env=env, check=True)
  return glob.glob(os.path.join(src, "board/obj/*/main.elf")), glob.glob(os.path.join(src, "board/obj/*/bootstub.elf"))


class TestRelease(unittest.TestCase):
  @classmethod
  def setUpClass(cls):
    cls.tmp = tempfile.TemporaryDirectory()
    cls.apps, cls.bootstubs = build(cls.tmp.name, True)
    cls.debug_elfs = sum(build(cls.tmp.name, False), [])

  @classmethod
  def tearDownClass(cls):
    cls.tmp.cleanup()

  def test_no_debug_features(self):
    self.assertTrue(self.apps)
    for elf in self.apps + self.bootstubs:
      self.assertFalse((DEBUG_SYMBOLS | DEBUG_REFS) & symbols(elf), elf)

  def test_debug_build_has_debug_features(self):
    found = set().union(*(symbols(elf) for elf in self.debug_elfs))
    self.assertEqual(DEBUG_SYMBOLS | DEBUG_REFS, (DEBUG_SYMBOLS | DEBUG_REFS) & found)

  def test_only_bootstub_programs_flash(self):
    for elf in self.apps:
      self.assertFalse(FLASH_SYMBOLS & symbols(elf), elf)
    for elf in self.bootstubs:
      self.assertTrue(FLASH_SYMBOLS & symbols(elf), elf)


if __name__ == "__main__":
  unittest.main()
