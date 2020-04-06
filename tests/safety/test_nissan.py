#!/usr/bin/env python3
import unittest
import numpy as np
from panda import Panda
from panda.tests.safety import libpandasafety_py
from panda.tests.safety.common import StdTest, make_msg, UNSAFE_MODE

ANGLE_DELTA_BP = [0., 5., 15.]
ANGLE_DELTA_V = [5., .8, .15]     # windup limit
ANGLE_DELTA_VU = [5., 3.5, 0.4]   # unwind limit

TX_MSGS = [[0x169, 0], [0x2b1, 0], [0x4cc, 0], [0x20b, 2], [0x280, 2]]

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


class TestNissanSafety(unittest.TestCase):
  @classmethod
  def setUp(cls):
    cls.safety = libpandasafety_py.libpandasafety
    cls.safety.set_safety_hooks(Panda.SAFETY_NISSAN, 0)
    cls.safety.init_tests_nissan()

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

  def _lkas_state_msg(self, state):
    to_send = make_msg(1, 0x30f)
    to_send[0].RDHR = (state & 0x1) << 3

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

  def _send_gas_cmd(self, gas):
    to_send = make_msg(0, 0x15c)
    to_send[0].RDHR = ((gas & 0x3fc) << 6) | ((gas & 0x3) << 22)

    return to_send

  def _acc_button_cmd(self, buttons):
    to_send = make_msg(2, 0x20b)
    to_send[0].RDLR = (buttons << 8)

    return to_send

  def test_spam_can_buses(self):
    StdTest.test_spam_can_buses(self, TX_MSGS)

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

  def test_brake_disengage(self):
    StdTest.test_allow_brake_at_zero_speed(self)
    StdTest.test_not_allow_brake_when_moving(self, 0)

  def test_gas_rising_edge(self):
    self.safety.set_controls_allowed(1)
    self.safety.safety_rx_hook(self._send_gas_cmd(100))
    self.assertFalse(self.safety.get_controls_allowed())

  def test_unsafe_mode_no_disengage_on_gas(self):
    self.safety.safety_rx_hook(self._send_gas_cmd(0))
    self.safety.set_controls_allowed(True)
    self.safety.set_unsafe_mode(UNSAFE_MODE.DISABLE_DISENGAGE_ON_GAS)
    self.safety.safety_rx_hook(self._send_gas_cmd(100))
    self.assertTrue(self.safety.get_controls_allowed())
    self.safety.set_unsafe_mode(UNSAFE_MODE.DEFAULT)

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

  def test_relay_malfunction(self):
    StdTest.test_relay_malfunction(self, 0x169)

  def test_fwd_hook(self):

    buss = list(range(0x0, 0x3))
    msgs = list(range(0x1, 0x800))

    blocked_msgs = [(2, 0x169), (2, 0x2b1), (2, 0x4cc), (0, 0x280)]
    for b in buss:
      for m in msgs:
        if b == 0:
          fwd_bus = 2
        elif b == 1:
          fwd_bus = -1
        elif b == 2:
          fwd_bus = 0

        if (b, m) in blocked_msgs:
          fwd_bus = -1

        # assume len 8
        self.assertEqual(fwd_bus, self.safety.safety_fwd_hook(b, make_msg(b, m, 8)))

if __name__ == "__main__":
  unittest.main()
