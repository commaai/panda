#!/usr/bin/env python3

import random
import unittest

from panda.tests.libpanda import libpanda_py

lpp = libpanda_py.libpanda

class TestLogging(unittest.TestCase):
  def setUp(self):
    # Clear buffer and setup state
    for i in range(lpp.logging_bank_size // 4):
      lpp.logging_bank[i] = 0xFFFFFFFF
    lpp.logging_init()

  def _get_logs(self):
    logs = []
    while True:
      log = libpanda_py.ffi.new("uint8_t[64]")
      if not lpp.logging_read(log):
        break
      logs.append(log)
    return logs

  def _log(self, msg):
    lpp.log(libpanda_py.ffi.new("char[]", msg.encode("utf-8")))

  def test_random_buffer_init(self):
    for i in range(lpp.logging_bank_size // 4):
      lpp.logging_bank[i] = random.randint(0, 0xFFFFFFFF)

    lpp.logging_init()

    for i in range(lpp.logging_bank_size // 4):
      assert lpp.logging_bank[i] == 0xFFFFFFFF
    self.assertFalse(lpp.logging_read(libpanda_py.ffi.new("uint8_t[64]")))

  def test_rate_limit(self):
    for _ in range(lpp.logging_rate_limit + 5):
      self._log("test")
    self.assertEqual(len(self._get_logs()), lpp.logging_rate_limit)
    self.assertEqual(len(self._get_logs()), 0)

    for _ in range(62):
      lpp.logging_tick()

    for _ in range(lpp.logging_rate_limit + 5):
      self._log("test")

    self.assertEqual(len(self._get_logs()), lpp.logging_rate_limit)
    self.assertEqual(len(self._get_logs()), 0)

  def test_logging(self):
    self._log("test")
    self.assertEqual(len(self._get_logs()), 1)

if __name__ == "__main__":
  unittest.main()