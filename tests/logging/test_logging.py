#!/usr/bin/env python3

import random
import unittest
import datetime

from panda import unpack_log
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
      if lpp.logging_read(log) == 0:
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
    self.assertEqual(lpp.logging_read(libpanda_py.ffi.new("uint8_t[64]")), 0)

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
    msg = "testing 123"
    self._log(msg)
    logs = self._get_logs()
    self.assertEqual(len(logs), 1)
    log = unpack_log(bytes(logs[0]))
    self.assertEqual(log['msg'], msg)
    self.assertEqual(log['uptime'], 0)
    self.assertEqual(log['id'], 1)
    self.assertEqual(log['timestamp'], datetime.datetime(1996, 4, 23, 4, 20, 20))

  def test_bank_overflow(self):
    for i in range(1, 10000):
      self._log("test")

      if i % 5 == 0:
        lpp.logging_init()
        expected_messages = (i % 2048) + (2048 if i >= 2048 else 0)
        self.assertEqual(len(self._get_logs()), expected_messages)

if __name__ == "__main__":
  unittest.main()