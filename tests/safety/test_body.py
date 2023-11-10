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

  def _torque_cmd_msg(self, torque_l, torque_r):
    values = {"TORQUE_L": torque_l, "TORQUE_R": torque_r}
    return self.packer.make_can_msg_panda("TORQUE_CMD", 0, values)

  def _max_motor_rpm_cmd_msg(self, max_rpm_l, max_rpm_r):
    values = {"MAX_RPM_L": max_rpm_l, "MAX_RPM_R": max_rpm_r}
    return self.packer.make_can_msg_panda("MAX_MOTOR_RPM_CMD", 0, values)

  def test_rx_hook(self):
    self.assertTrue(self._rx(self._motors_data_msg(0, 0)))

  def test_tx_hook(self):
    self.assertFalse(self._tx(self._torque_cmd_msg(0, 0)))
    self.safety.set_controls_allowed(True)
    self.assertTrue(self._tx(self._torque_cmd_msg(0, 0)))

  # TODO: test addr checks (timestep)

  def test_can_flasher_msg(self):
    # CAN flasher always allowed
    self.assertTrue(self._tx(common.make_msg(0, 0x1, 8)))

    self.safety.set_controls_allowed(False)
    self.assertFalse(self._tx(common.make_msg(0, 0x2, 8)))
    # TODO: what is 0xdeadfaceU and 0x0ab00b1eU? no address check?
    self.assertTrue(self._tx(common.make_msg(0, 0x2, dat=b'\xce\xfa\xad\xde\x1e\x0b\xb0\x0a')))



# TODO: add knee tests


if __name__ == "__main__":
  unittest.main()
