#!/usr/bin/env python3
import unittest
from panda import Panda
from panda.tests.libpanda import libpanda_py
import panda.tests.safety.common as common
from panda.tests.safety.common import CANPackerPanda


class TestChryslerCusw_Safety(common.PandaCarSafetyTest, common.MotorTorqueSteeringSafetyTest):
  TX_MSGS = [[0x1F6, 0], [0x2FA, 0], [0x5DC, 0]]
  STANDSTILL_THRESHOLD = 0
  RELAY_MALFUNCTION_ADDRS = {0: (0x1F6,)}
  FWD_BLACKLISTED_ADDRS = {2: [0x1F6, 0x5DC]}
  FWD_BUS_LOOKUP = {0: 2, 2: 0}

  MAX_RATE_UP = 3
  MAX_RATE_DOWN = 3
  MAX_TORQUE = 261
  MAX_RT_DELTA = 112
  RT_INTERVAL = 250000
  MAX_TORQUE_ERROR = 80

  def setUp(self):
    self.packer = CANPackerPanda("chrysler_cusw")
    self.safety = libpanda_py.libpanda
    self.safety.set_safety_hooks(Panda.SAFETY_CHRYSLER_CUSW, 0)
    self.safety.init_tests()

  def _button_msg(self, cancel=False, resume=False):
    values = {"ACC_Cancel": cancel, "ACC_Resume": resume}
    return self.packer.make_can_msg_panda("CRUISE_BUTTONS", 0, values)

  def _pcm_status_msg(self, enable):
    values = {"ACC_STATE": 4 if enable else 3}
    return self.packer.make_can_msg_panda("ACC_1", 0, values)

  def _speed_msg(self, speed):
    values = {"VEHICLE_SPEED": speed}
    return self.packer.make_can_msg_panda("BRAKE_1", 0, values)

  def _user_gas_msg(self, gas):
    values = {"GAS_HUMAN": gas}
    return self.packer.make_can_msg_panda("ACCEL_GAS", 0, values)

  def _user_brake_msg(self, brake):
    values = {"BRAKE_HUMAN": 1 if brake else 0}
    return self.packer.make_can_msg_panda("BRAKE_2", 0, values)

  def _torque_meas_msg(self, torque):
    values = {"TORQUE_MOTOR": torque}
    return self.packer.make_can_msg_panda("EPS_STATUS", 0, values)

  def _torque_cmd_msg(self, torque, steer_req=1):
    values = {"STEERING_TORQUE": torque, "LKAS_CONTROL_BIT": steer_req}
    return self.packer.make_can_msg_panda("LKAS_COMMAND", 0, values)

  def test_buttons(self):
    for controls_allowed in (True, False):
      self.safety.set_controls_allowed(controls_allowed)

      # resume only while controls allowed
      self.assertEqual(controls_allowed, self._tx(self._button_msg(resume=True)))

      # can always cancel
      self.assertTrue(self._tx(self._button_msg(cancel=True)))


if __name__ == "__main__":
  unittest.main()
