#!/usr/bin/env python2
import unittest
import numpy as np
import libpandasafety_py

MAX_RATE_UP = 10
MAX_RATE_DOWN = 25
MAX_TORQUE = 1500

MAX_ACCEL = 1500
MIN_ACCEL = -3000


def twos_comp(val, bits):
  if val >= 0:
    return val
  else:
    return (2**bits) + val


class TestToyotaSafety(unittest.TestCase):
  @classmethod
  def setUpClass(cls):
    cls.safety = libpandasafety_py.libpandasafety
    cls.safety.toyota_init(100)
    cls.safety.init_tests_toyota()

  def _set_prev_torque(self, t):
    self.safety.set_desired_torque_last(t)
    self.safety.set_rt_torque_last(t)
    self.safety.set_torque_meas(t, t)

  def _torque_msg(self, torque):
    to_send = libpandasafety_py.ffi.new('CAN_FIFOMailBox_TypeDef *')
    to_send[0].RIR = 0x2E4 << 21

    t = twos_comp(torque, 16)
    to_send[0].RDLR = t | ((t & 0xFF) << 16)
    return to_send

  def _accel_msg(self, accel):
    to_send = libpandasafety_py.ffi.new('CAN_FIFOMailBox_TypeDef *')
    to_send[0].RIR = 0x343 << 21

    a = twos_comp(accel, 16)
    to_send[0].RDLR = (a & 0xFF) << 8 | (a >> 8)
    return to_send

  def test_default_controls_not_allowed(self):
    self.assertFalse(self.safety.get_controls_allowed())

  def test_manually_enable_controls_allowed(self):
    self.safety.set_controls_allowed(1)
    self.assertTrue(self.safety.get_controls_allowed())

  def test_enable_control_allowed_from_cruise(self):
    to_push = libpandasafety_py.ffi.new('CAN_FIFOMailBox_TypeDef *')
    to_push[0].RIR = 0x1D2 << 21
    to_push[0].RDHR = 0xF00000

    self.safety.toyota_rx_hook(to_push)
    self.assertTrue(self.safety.get_controls_allowed())

  def test_accel_actuation_limits(self):
    for accel in np.arange(-4000, 4000, 100):
      for controls_allowed in [True, False]:
        self.safety.set_controls_allowed(controls_allowed)

        if controls_allowed:
          send = MIN_ACCEL <= accel <= MAX_ACCEL
        else:
          send = accel == 0
        self.assertEqual(send, self.safety.toyota_tx_hook(self._accel_msg(accel)))

  def test_torque_absolute_limits(self):
    for controls_allowed in [True, False]:
      for torque in np.arange(2000, 2000, MAX_RATE_UP):
          self.safety.set_controls_allowed(controls_allowed)
          self.safety.set_rt_torque_last(torque)
          self.safety.set_torque_meas(torque, torque)
          self.safety.set_desired_torque_last(torque - MAX_RATE_UP)

          if controls_allowed:
            send = (-MAX_TORQUE <= torque <= MAX_TORQUE)
          else:
            send = torque == 0

          print controls_allowed, torque
          self.assertEqual(send, self.safety.toyota_tx_hook(self._torque_msg(torque)))

  def test_non_realtime_limit_up(self):
    self.safety.set_controls_allowed(True)

    self._set_prev_torque(0)
    self.assertTrue(self.safety.toyota_tx_hook(self._torque_msg(MAX_RATE_UP)))

    self._set_prev_torque(0)
    self.assertFalse(self.safety.toyota_tx_hook(self._torque_msg(MAX_RATE_UP + 1)))

  def test_non_realtime_limit_down(self):
    self.safety.set_controls_allowed(True)

    self._set_prev_torque(100)
    self.assertTrue(self.safety.toyota_tx_hook(self._torque_msg(100 - MAX_RATE_DOWN)))

    self._set_prev_torque(100)
    self.assertFalse(self.safety.toyota_tx_hook(self._torque_msg(100 - MAX_RATE_DOWN - 1)))

if __name__ == "__main__":
  unittest.main()
