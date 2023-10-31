#!/usr/bin/env python3
import unittest

import panda.tests.safety.common as common

from panda import Panda
from panda.tests.libpanda import libpanda_py


class TestElm327(common.PandaSafetyTest):
  # TX_MSGS = [(addr, bus) for addr in range(0x600, 0x800) for bus in range(4)] + \
  #           [(addr, bus) for addr in [0x18DB33F1, *range(0x18DA00F1, 0x18DB00F1, 0x100)] for bus in range(4)]

  TX_MSGS = [(addr, bus) for addr in [*range(0x600, 0x800),
                                      *range(0x18DA00F1, 0x18DB00F1, 0x100),  # 29-bit UDS physical addressing
                                      *[0x18DB33F1],  # 29-bit UDS functional address
                                      ] for bus in range(4)]

  def setUp(self):
    self.safety = libpanda_py.libpanda
    self.safety.set_safety_hooks(Panda.SAFETY_ELM327, 0)
    self.safety.init_tests()

  def test_tx_hook(self):
    self.TX_MSGS = set(self.TX_MSGS)
    for bus in range(4):
      for addr in self.SCANNED_ADDRS:
        # self._tx(common.make_msg(bus, addr, 8))
        # continue
        # ELM only allows 8 byte UDS/KWP messages under ISO 15765-4
        should_tx = (addr, bus) in self.TX_MSGS# and l == 8
        for l in list(range(8 + 1)) + [16, 32, 64]:
          self.assertEqual(should_tx and l == 8, self._tx(common.make_msg(bus, addr, l)))

    # for addr, bus in self.TX_MSGS:
    #   self._tx(common.make_msg(bus, addr, 8))
    #   continue
    #   self.assertTrue(self._tx(common.make_msg(bus, addr, 8)), f"not allowed TX {addr=} {bus=}")
    #   for l in range(8 + 1):
    #     self.assertFalse(self._tx(common.make_msg(bus, addr, l)), f"allowed TX {addr=} {bus=}")

  def test_rx_hook(self):
    for addr in self.SCANNED_ADDRS:
      self.assertTrue(self._rx(common.make_msg(0, addr, 8)), f"not allowed RX {addr=}")

  def test_tx_lin_hook(self):
    for lin_num in range(2):
      self.assertFalse(self._tx_lin(lin_num, b'\x00' * 8, 8), f"allowed TX LIN {lin_num=}")

  def test_tx_hook_on_wrong_safety_mode(self):
    # TODO: re-enable me
    pass

#
# class TestAllOutput(common.PandaSafetyTest):
#   # Allow all messages
#   TX_MSGS = [(addr, bus) for addr in common.PandaSafetyTest.SCANNED_ADDRS
#              for bus in range(4)]
#
#   def setUp(self):
#     self.safety = libpanda_py.libpanda
#     self.safety.set_safety_hooks(Panda.SAFETY_ALLOUTPUT, 0)
#     self.safety.init_tests()
#
#   def test_rx_hook(self):
#     for addr, bus in self.TX_MSGS:
#       self.assertTrue(self._rx(common.make_msg(bus, addr, 8)), f"not allowed RX {addr=}")
#
#   def test_tx_lin_hook(self):
#     for lin_num in range(2):
#       self.assertTrue(self._tx_lin(lin_num, b'\x00' * 8, 8), f"allowed TX LIN {lin_num=}")
#
#   def test_spam_can_buses(self):
#     # Uses TX_MSGS instead of scanned addrs and asserts all send
#     for bus in range(4):
#       for addr, bus in self.TX_MSGS:
#         self.assertTrue(self._tx(common.make_msg(bus, addr, 8)), f"not allowed TX {addr=} {bus=}")
#
#   def test_default_controls_not_allowed(self):
#     self.assertTrue(self.safety.get_controls_allowed())
#
#   def test_tx_hook_on_wrong_safety_mode(self):
#     # No point, since we allow all messages
#     pass
#
#
# class TestAllOutputPassthrough(TestAllOutput):
#   FWD_BLACKLISTED_ADDRS = {}
#   FWD_BUS_LOOKUP = {0: 2, 2: 0}
#
#   def setUp(self):
#     self.safety = libpanda_py.libpanda
#     self.safety.set_safety_hooks(Panda.SAFETY_ALLOUTPUT, 1)
#     self.safety.init_tests()


if __name__ == "__main__":
  unittest.main()
