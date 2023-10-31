#!/usr/bin/env python3
import unittest

import panda.tests.safety.common as common

from panda import Panda
from panda.tests.libpanda import libpanda_py
from panda.tests.safety.common import CANPackerPanda


class TestBody(common.PandaSafetyTest):
  TX_MSGS = [[0x250, 0], [0x251, 0], [0x350, 0], [0x351, 0],
             [0x1, 0], [0x1, 1], [0x1, 2], [0x1, 3]]

  def setUp(self):
    self.packer = CANPackerPanda("comma_body")
    self.safety = libpanda_py.libpanda
    self.safety.set_safety_hooks(Panda.SAFETY_BODY, 0)
    self.safety.init_tests()

  def _motors_data_msg(self, speed_l, speed_r):
    values = {"SPEED_L": speed_l, "SPEED_R": speed_r}
    return self.packer.make_can_msg_panda("MOTORS_DATA", 0, values)

  def test_rx_hook(self):
    self.assertTrue(self._rx(self._motors_data_msg(0, 0)))


if __name__ == "__main__":
  unittest.main()
