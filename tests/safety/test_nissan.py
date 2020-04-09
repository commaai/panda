#!/usr/bin/env python3
import unittest
import numpy as np
from panda import Panda
from panda.tests.safety import libpandasafety_py
from panda.tests.safety.common import PandaSafetyTest, CANPackerPanda, UNSAFE_MODE

ANGLE_DELTA_BP = [0., 5., 15.]
ANGLE_DELTA_V = [5., .8, .15]     # windup limit
ANGLE_DELTA_VU = [5., 3.5, 0.4]   # unwind limit

def sign(a):
  if a > 0:
    return 1
  else:
    return -1


class TestNissanSafety(PandaSafetyTest, unittest.TestCase):
  TX_MSGS = [[0x169, 0], [0x2b1, 0], [0x4cc, 0], [0x20b, 2], [0x280, 2]]
  STANDSTILL_THRESHOLD = 0
  RELAY_MALFUNCTION_ADDR = 0x169
  RELAY_MALFUNCTION_BUS = 0
  FWD_BLACKLISTED_ADDRS = {2: [0x169, 0x2b1, 0x4cc], 0: [0x280]}
  FWD_BUS_LOOKUP = {0: 2, 2: 0}

  @classmethod
  def setUp(cls):
    cls.packer = CANPackerPanda("nissan_x_trail_2017")
    cls.safety = libpandasafety_py.libpandasafety
    cls.safety.set_safety_hooks(Panda.SAFETY_NISSAN, 0)
    cls.safety.init_tests_nissan()

  def _angle_meas_msg(self, angle):
    values = {"STEER_ANGLE": angle}
    return self.packer.make_can_msg_panda("STEER_ANGLE_SENSOR", 0, values)

  def _set_prev_angle(self, t):
    t = int(t * -100)
    self.safety.set_nissan_desired_angle_last(t)

  def _angle_meas_msg_array(self, angle):
    for i in range(6):
      self._rx(self._angle_meas_msg(angle))

  def _lkas_state_msg(self, state):
    values = {"CRUISE_ENABLED": state}
    return self.packer.make_can_msg_panda("CRUISE_STATE", 0, values)

  def _lkas_control_msg(self, angle, state):
    values = {"DESIRED_ANGLE": angle, "LKA_ACTIVE": state}
    return self.packer.make_can_msg_panda("LKAS", 0, values)

  def _speed_msg(self, speed):
    # TODO: why is this extra scale factor in panda safety but not in the dbc?
    speed = int(speed * 3.6)
    values = {"WHEEL_SPEED_%s"%s: speed for s in ["RR", "RL"]}
    return self.packer.make_can_msg_panda("WHEEL_SPEEDS_REAR", 0, values)

  def _brake_msg(self, brake):
    values = {"USER_BRAKE_PRESSED": brake}
    return self.packer.make_can_msg_panda("DOORS_LIGHTS", 0, values)

  def _send_gas_cmd(self, gas):
    values = {"GAS_PEDAL": gas}
    return self.packer.make_can_msg_panda("GAS_PEDAL", 0, values)

  def _acc_button_cmd(self, cancel=0, propilot=0, res=0, _set=0, follow_distance=0):
    no_button = not any([cancel, propilot, res, _set, follow_distance])
    values = {"PROPILOT_BUTTON": propilot, "CANCEL_BUTTON": cancel, "SET_BUTTON": _set,
                "RES_BUTTON": res, "FOLLOW_DISTANCE_BUTTON": follow_distance, "NO_BUTTON_PRESSED": no_button}
    return self.packer.make_can_msg_panda("CRUISE_THROTTLE", 2, values)

  def test_angle_cmd_when_enabled(self):
    # when controls are allowed, angle cmd rate limit is enforced
    speeds = [0., 1., 5., 10., 15., 50.]
    angles = [-300, -100, -10, 0, 10, 100, 300]
    for a in angles:
      for s in speeds:
        max_delta_up = np.interp(s, ANGLE_DELTA_BP, ANGLE_DELTA_V)
        max_delta_down = np.interp(s, ANGLE_DELTA_BP, ANGLE_DELTA_VU)

        # first test against false positives
        self._angle_meas_msg_array(a)
        self._rx(self._speed_msg(s))

        self._set_prev_angle(a)
        self.safety.set_controls_allowed(1)

        # Stay within limits
        # Up
        self.assertEqual(True, self._tx(self._lkas_control_msg(a + sign(a) * max_delta_up, 1)))
        self.assertTrue(self.safety.get_controls_allowed())

        # Don't change
        self.assertEqual(True, self._tx(self._lkas_control_msg(a, 1)))
        self.assertTrue(self.safety.get_controls_allowed())

        # Down
        self.assertEqual(True, self._tx(self._lkas_control_msg(a - sign(a) * max_delta_down, 1)))
        self.assertTrue(self.safety.get_controls_allowed())

        # Inject too high rates
        # Up
        self.assertEqual(False, self._tx(self._lkas_control_msg(a + sign(a) * (max_delta_up + 1), 1)))
        self.assertFalse(self.safety.get_controls_allowed())

        # Don't change
        self.safety.set_controls_allowed(1)
        self._set_prev_angle(a)
        self.assertTrue(self.safety.get_controls_allowed())
        self.assertEqual(True, self._tx(self._lkas_control_msg(a, 1)))
        self.assertTrue(self.safety.get_controls_allowed())

        # Down
        self.assertEqual(False, self._tx(self._lkas_control_msg(a - sign(a) * (max_delta_down + 1), 1)))
        self.assertFalse(self.safety.get_controls_allowed())

        # Check desired steer should be the same as steer angle when controls are off
        self.safety.set_controls_allowed(0)
        self.assertEqual(True, self._tx(self._lkas_control_msg(a, 0)))

  def test_angle_cmd_when_disabled(self):
    self.safety.set_controls_allowed(0)

    self._set_prev_angle(0)
    self.assertFalse(self._tx(self._lkas_control_msg(0, 1)))
    self.assertFalse(self.safety.get_controls_allowed())

  def test_gas_rising_edge(self):
    self.safety.set_controls_allowed(1)
    self._rx(self._send_gas_cmd(100))
    self.assertFalse(self.safety.get_controls_allowed())

  def test_unsafe_mode_no_disengage_on_gas(self):
    self._rx(self._send_gas_cmd(0))
    self.safety.set_controls_allowed(True)
    self.safety.set_unsafe_mode(UNSAFE_MODE.DISABLE_DISENGAGE_ON_GAS)
    self._rx(self._send_gas_cmd(100))
    self.assertTrue(self.safety.get_controls_allowed())
    self.safety.set_unsafe_mode(UNSAFE_MODE.DEFAULT)

  def test_acc_buttons(self):
    self.safety.set_controls_allowed(1)
    self._tx(self._acc_button_cmd(cancel=1)) # Cancel button
    self.assertTrue(self.safety.get_controls_allowed())
    self._tx(self._acc_button_cmd(propilot=1)) # ProPilot button
    self.assertFalse(self.safety.get_controls_allowed())
    self.safety.set_controls_allowed(1)
    self._tx(self._acc_button_cmd(follow_distance=1)) # Follow Distance button
    self.assertFalse(self.safety.get_controls_allowed())
    self.safety.set_controls_allowed(1)
    self._tx(self._acc_button_cmd(_set=1)) # Set button
    self.assertFalse(self.safety.get_controls_allowed())
    self.safety.set_controls_allowed(1)
    self._tx(self._acc_button_cmd(res=1)) # Res button
    self.assertFalse(self.safety.get_controls_allowed())
    self.safety.set_controls_allowed(1)
    self._tx(self._acc_button_cmd()) # No button pressed
    self.assertFalse(self.safety.get_controls_allowed())


if __name__ == "__main__":
  unittest.main()
