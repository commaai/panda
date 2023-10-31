#!/usr/bin/env python3
import unittest

import panda.tests.safety.common as common

from panda import Panda
from panda.tests.libpanda import libpanda_py


class TestNoOutput(common.PandaSafetyTest):
  TX_MSGS = []

  def setUp(self):
    self.safety = libpanda_py.libpanda
    self.safety.set_safety_hooks(Panda.SAFETY_NOOUTPUT, 0)
    self.safety.init_tests()


class TestAllOutput(common.PandaSafetyTest):
  # Allow all messages
  TX_MSGS = [(addr, bus) for addr in common.PandaSafetyTest.SCANNED_ADDRS
             for bus in range(4)]

  def setUp(self):
    self.safety = libpanda_py.libpanda
    self.safety.set_safety_hooks(Panda.SAFETY_ALLOUTPUT, 0)
    self.safety.init_tests()

  def test_spam_can_buses(self):
    # Uses TX_MSGS instead of scanned addrs and asserts all send
    for bus in range(4):
      for addr, bus in self.TX_MSGS:
        self.assertTrue(self._tx(common.make_msg(bus, addr, 8)), f"not allowed TX {addr=} {bus=}")

  def test_default_controls_not_allowed(self):
    self.assertTrue(self.safety.get_controls_allowed())

  def test_tx_hook_on_wrong_safety_mode(self):
    # No point, since we allow all messages
    pass


class TestAllOutputPassthrough(TestAllOutput):
  FWD_BLACKLISTED_ADDRS = {}
  FWD_BUS_LOOKUP = {0: 2, 2: 0}

  def setUp(self):
    self.safety = libpanda_py.libpanda
    self.safety.set_safety_hooks(Panda.SAFETY_ALLOUTPUT, 1)
    self.safety.init_tests()


if __name__ == "__main__":
  unittest.main()
