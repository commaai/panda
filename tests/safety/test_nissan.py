#!/usr/bin/env python3
import unittest
import numpy as np
from panda import Panda
from panda.tests.safety import libpandasafety_py
import panda.tests.safety.common as common
from panda.tests.safety.common import CANPackerPanda, make_msg

ANGLE_DELTA_BP = [0., 5., 15.]
ANGLE_DELTA_V = [5., .8, .15]     # windup limit
ANGLE_DELTA_VU = [5., 3.5, 0.4]   # unwind limit


def twos_comp(val, bits):
  if val >= 0:
    return val
  else:
    return (2**bits) + val

def sign(a):
  if a > 0:
    return 1
  else:
    return -1


class TestNissanSafety(common.PandaSafetyTest):

  TX_MSGS = [[0x169, 0], [0x2b1, 0], [0x4cc, 0], [0x20b, 2], [0x280, 2]]
  STANDSTILL_THRESHOLD = 0
  GAS_PRESSED_THRESHOLD = 1
  RELAY_MALFUNCTION_ADDR = 0x169
  RELAY_MALFUNCTION_BUS = 0
  FWD_BLACKLISTED_ADDRS = {0: [0x280], 2: [0x169, 0x2b1, 0x4cc]}
  FWD_BUS_LOOKUP = {0: 2, 2: 0}

  def setUp(self):
    self.packer = CANPackerPanda("nissan_x_trail_2017")
    self.safety = libpandasafety_py.libpandasafety
    self.safety.set_safety_hooks(Panda.SAFETY_NISSAN, 0)
    self.safety.init_tests_nissan()

  def _angle_meas_msg(self, angle):
    to_send = make_msg(0, 0x2)
    angle = int(angle * -10)
    t = twos_comp(angle, 16)
    to_send[0].RDLR = t & 0xFFFF
    return to_send

  def _set_prev_angle(self, t):
    t = int(t * -100)
    self.safety.set_nissan_desired_angle_last(t)

  def _angle_meas_msg_array(self, angle):
    for i in range(6):
      self.safety.safety_rx_hook(self._angle_meas_msg(angle))

  def _pcm_status_msg(self, enabled):
    to_send = make_msg(2, 0x30f)
    to_send[0].RDLR = (1 if enabled else 0) << 3
    return to_send

  def _lkas_control_msg(self, angle, state):
    to_send = make_msg(0, 0x169)
    angle = int((angle - 1310) * -100)
    to_send[0].RDLR = ((angle & 0x3FC00) >> 10) | ((angle & 0x3FC) << 6) | ((angle & 0x3) << 16)
    to_send[0].RDHR = ((state & 0x1) << 20)
    return to_send

  def _speed_msg(self, speed):
    to_send = make_msg(0, 0x285)
    speed = int(speed / 0.005 * 3.6)
    to_send[0].RDLR = ((speed & 0xFF) << 24) | ((speed & 0xFF00) << 8)
    return to_send

  def _brake_msg(self, brake):
    to_send = make_msg(1, 0x454)
    to_send[0].RDLR = ((brake & 0x1) << 23)
    return to_send

  def _gas_msg(self, gas):
    to_send = make_msg(0, 0x15c)
    to_send[0].RDHR = ((gas & 0x3fc) << 6) | ((gas & 0x3) << 22)
    return to_send

  def _acc_button_cmd(self, buttons):
    to_send = make_msg(2, 0x20b)
    to_send[0].RDLR = (buttons << 8)
    return to_send

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
        self.safety.safety_rx_hook(self._speed_msg(s))

        self._set_prev_angle(a)
        self.safety.set_controls_allowed(1)

        # Stay within limits
        # Up
        self.assertEqual(True, self.safety.safety_tx_hook(self._lkas_control_msg(a + sign(a) * max_delta_up, 1)))
        self.assertTrue(self.safety.get_controls_allowed())

        # Don't change
        self.assertEqual(True, self.safety.safety_tx_hook(self._lkas_control_msg(a, 1)))
        self.assertTrue(self.safety.get_controls_allowed())

        # Down
        self.assertEqual(True, self.safety.safety_tx_hook(self._lkas_control_msg(a - sign(a) * max_delta_down, 1)))
        self.assertTrue(self.safety.get_controls_allowed())

        # Inject too high rates
        # Up
        self.assertEqual(False, self.safety.safety_tx_hook(self._lkas_control_msg(a + sign(a) * (max_delta_up + 1), 1)))
        self.assertFalse(self.safety.get_controls_allowed())

        # Don't change
        self.safety.set_controls_allowed(1)
        self._set_prev_angle(a)
        self.assertTrue(self.safety.get_controls_allowed())
        self.assertEqual(True, self.safety.safety_tx_hook(self._lkas_control_msg(a, 1)))
        self.assertTrue(self.safety.get_controls_allowed())

        # Down
        self.assertEqual(False, self.safety.safety_tx_hook(self._lkas_control_msg(a - sign(a) * (max_delta_down + 1), 1)))
        self.assertFalse(self.safety.get_controls_allowed())

        # Check desired steer should be the same as steer angle when controls are off
        self.safety.set_controls_allowed(0)
        self.assertEqual(True, self.safety.safety_tx_hook(self._lkas_control_msg(a, 0)))

  def test_angle_cmd_when_disabled(self):
    self.safety.set_controls_allowed(0)

    self._set_prev_angle(0)
    self.assertFalse(self.safety.safety_tx_hook(self._lkas_control_msg(0, 1)))
    self.assertFalse(self.safety.get_controls_allowed())

  def test_acc_buttons(self):
    self.safety.set_controls_allowed(1)
    self.safety.safety_tx_hook(self._acc_button_cmd(0x2)) # Cancel button
    self.assertTrue(self.safety.get_controls_allowed())
    self.safety.safety_tx_hook(self._acc_button_cmd(0x1)) # ProPilot button
    self.assertFalse(self.safety.get_controls_allowed())
    self.safety.set_controls_allowed(1)
    self.safety.safety_tx_hook(self._acc_button_cmd(0x4)) # Follow Distance button
    self.assertFalse(self.safety.get_controls_allowed())
    self.safety.set_controls_allowed(1)
    self.safety.safety_tx_hook(self._acc_button_cmd(0x8)) # Set button
    self.assertFalse(self.safety.get_controls_allowed())
    self.safety.set_controls_allowed(1)
    self.safety.safety_tx_hook(self._acc_button_cmd(0x10)) # Res button
    self.assertFalse(self.safety.get_controls_allowed())
    self.safety.set_controls_allowed(1)
    self.safety.safety_tx_hook(self._acc_button_cmd(0x20)) # No button pressed
    self.assertFalse(self.safety.get_controls_allowed())


if __name__ == "__main__":
  unittest.main()
