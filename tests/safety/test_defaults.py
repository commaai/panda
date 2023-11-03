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

  def test_rx_hook(self):
    for addr in self.SCANNED_ADDRS:
      self.assertTrue(self._rx(common.make_msg(0, addr, 8)), f"not allowed RX {addr=}")

  def test_tx_lin_hook(self):
    for lin_num in range(2):
      self.assertFalse(self._tx_lin(lin_num, b'\x00' * 8, 8), f"allowed TX LIN {lin_num=}")


class TestAllOutput(common.PandaSafetyTest):
  # Allow all messages
  TX_MSGS = [[addr, bus] for addr in common.PandaSafetyTest.SCANNED_ADDRS
             for bus in range(4)]

  def setUp(self):
    self.safety = libpanda_py.libpanda
    self.safety.set_safety_hooks(Panda.SAFETY_ALLOUTPUT, 0)
    self.safety.init_tests()

  def test_rx_hook(self):
    for addr, bus in self.TX_MSGS:
      self.assertTrue(self._rx(common.make_msg(bus, addr, 8)), f"not allowed RX {addr=}")

  def test_tx_lin_hook(self):
    for lin_num in range(2):
      self.assertTrue(self._tx_lin(lin_num, b'\x00' * 8, 8), f"allowed TX LIN {lin_num=}")

  def test_spam_can_buses(self):
    # Uses TX_MSGS instead of scanned addrs and asserts all send
    for addr, bus in self.TX_MSGS:
      self.assertTrue(self._tx(common.make_msg(bus, addr, 8)), f"not allowed TX {addr=} {bus=}")

  def test_default_controls_not_allowed(self):
    # controls always allowed
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