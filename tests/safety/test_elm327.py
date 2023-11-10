#!/usr/bin/env python3
import unittest

import panda.tests.safety.common as common

from panda import DLC_TO_LEN, Panda
from panda.tests.libpanda import libpanda_py
from panda.tests.safety.test_defaults import TestDefaultRxHookBase


class TestElm327(TestDefaultRxHookBase):
  TX_MSGS = [[addr, bus] for addr in [*range(0x600, 0x800),
                                      *range(0x18DA00F1, 0x18DB00F1, 0x100),  # 29-bit UDS physical addressing
                                      *[0x18DB33F1],  # 29-bit UDS functional address
                                      ] for bus in range(4)]

  def setUp(self):
    self.safety = libpanda_py.libpanda
    self.safety.set_safety_hooks(Panda.SAFETY_ELM327, 0)
    self.safety.init_tests()

  def test_tx_hook(self):
    # ensure we can transmit arbitrary data on allowed addresses
    for bus in range(4):
      for addr in self.SCANNED_ADDRS:
        should_tx = [addr, bus] in self.TX_MSGS
        self.assertEqual(should_tx, self._tx(common.make_msg(bus, addr, 8)))

    # ELM only allows 8 byte UDS/KWP messages under ISO 15765-4
    for msg_len in DLC_TO_LEN:
      should_tx = msg_len == 8
      self.assertEqual(should_tx, self._tx(common.make_msg(0, 0x700, msg_len)))

  def test_tx_lin_hook(self):
    # spot check some cases
    self.assertFalse(self._tx_lin(0xC0, 1, 0x33, 0xF1, b'\x01\x0F'))  # wrong lin number/bus
    self.assertFalse(self._tx_lin(0xC0, 0, 0x33, 0xF1, b''))  # wrong length
    self.assertFalse(self._tx_lin(0xB0, 0, 0x33, 0xF1, b'\x01\x0E'))  # bad priority
    self.assertFalse(self._tx_lin(0xC0, 0, 0x00, 0xF1, b'\x01\x0D'))  # bad addr
    self.assertFalse(self._tx_lin(0xC0, 0, 0x33, 0x00, b'\x01\x0C'))  # bad addr

    for msg_len in range(8 + 1):
      # first three bytes are made up of priority, len, rx/tx addresses
      # payload is not checked, try sending 0xFF
      should_tx = 2 <= (msg_len) <= 7
      self.assertEqual(should_tx, self._tx_lin(0xC0, 0, 0x33, 0xF1, b'\xFF' * msg_len))

  def test_tx_hook_on_wrong_safety_mode(self):
    # No point, since we allow many diagnostic addresses
    pass


if __name__ == "__main__":
  unittest.main()
