#!/usr/bin/env python2
import unittest
import numpy as np
import libpandasafety_py

MAX_RATE_UP = 10
MAX_RATE_DOWN = 25
MAX_TORQUE = 1500

MAX_ACCEL = 1500
MIN_ACCEL = -3000

MAX_RT_DELTA = 375
RT_INTERVAL = 250000

MAX_TORQUE_ERROR = 350

def twos_comp(val, bits):
  if val >= 0:
    return val
  else:
    return (2**bits) + val


class TestToyotaSafety(unittest.TestCase):
  @classmethod
  def setUp(cls):
    cls.safety = libpandasafety_py.libpandasafety
    cls.safety.toyota_init(100)
    cls.safety.init_tests_toyota()

  def _set_prev_torque(self, t):
    self.safety.set_desired_torque_last(t)
    self.safety.set_rt_torque_last(t)
    self.safety.set_torque_meas(t, t)

  def _torque_meas_msg(self, torque):
    to_send = libpandasafety_py.ffi.new('CAN_FIFOMailBox_TypeDef *')
    to_send[0].RIR = 0x260 << 21

    t = twos_comp(torque, 16)
    to_send[0].RDHR = t | ((t & 0xFF) << 16)
    return to_send

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

  def test_disable_control_allowed_from_cruise(self):
    to_push = libpandasafety_py.ffi.new('CAN_FIFOMailBox_TypeDef *')
    to_push[0].RIR = 0x1D2 << 21
    to_push[0].RDHR = 0

    self.safety.set_controls_allowed(1)
    self.safety.toyota_rx_hook(to_push)
    self.assertFalse(self.safety.get_controls_allowed())

  def test_accel_actuation_limits(self):
    for accel in np.arange(MIN_ACCEL - 1000, MAX_ACCEL + 1000, 100):
      for controls_allowed in [True, False]:
        self.safety.set_controls_allowed(controls_allowed)

        if controls_allowed:
          send = MIN_ACCEL <= accel <= MAX_ACCEL
        else:
          send = accel == 0
        self.assertEqual(send, self.safety.toyota_tx_hook(self._accel_msg(accel)))

  def test_torque_absolute_limits(self):
    for controls_allowed in [True, False]:
      for torque in np.arange(-MAX_TORQUE - 1000, MAX_TORQUE + 1000, MAX_RATE_UP):
          self.safety.set_controls_allowed(controls_allowed)
          self.safety.set_rt_torque_last(torque)
          self.safety.set_torque_meas(torque, torque)
          self.safety.set_desired_torque_last(torque - MAX_RATE_UP)

          if controls_allowed:
            send = (-MAX_TORQUE <= torque <= MAX_TORQUE)
          else:
            send = torque == 0

          self.assertEqual(send, self.safety.toyota_tx_hook(self._torque_msg(torque)))

  def test_non_realtime_limit_up(self):
    self.safety.set_controls_allowed(True)

    self._set_prev_torque(0)
    self.assertTrue(self.safety.toyota_tx_hook(self._torque_msg(MAX_RATE_UP)))

    self._set_prev_torque(0)
    self.assertFalse(self.safety.toyota_tx_hook(self._torque_msg(MAX_RATE_UP + 1)))

  def test_non_realtime_limit_down(self):
    self.safety.set_controls_allowed(True)

    self.safety.set_rt_torque_last(1000)
    self.safety.set_torque_meas(500, 500)
    self.safety.set_desired_torque_last(1000)
    self.assertTrue(self.safety.toyota_tx_hook(self._torque_msg(1000 - MAX_RATE_DOWN)))

    self.safety.set_rt_torque_last(1000)
    self.safety.set_torque_meas(500, 500)
    self.safety.set_desired_torque_last(1000)
    self.assertFalse(self.safety.toyota_tx_hook(self._torque_msg(1000 - MAX_RATE_DOWN + 1)))

  def test_exceed_torque_sensor(self):
    self.safety.set_controls_allowed(True)

    for sign in [-1, 1]:
      self._set_prev_torque(0)
      for t in np.arange(0, MAX_TORQUE_ERROR + 10, 10):
        t *= sign
        self.assertTrue(self.safety.toyota_tx_hook(self._torque_msg(t)))

      self.assertFalse(self.safety.toyota_tx_hook(self._torque_msg(sign * (MAX_TORQUE_ERROR + 10))))

  def test_realtime_limit_up(self):
    self.safety.set_controls_allowed(True)

    for sign in [-1, 1]:
      self.safety.init_tests_toyota()
      self._set_prev_torque(0)
      for t in np.arange(0, 380, 10):
        t *= sign
        self.safety.set_torque_meas(t, t)
        self.assertTrue(self.safety.toyota_tx_hook(self._torque_msg(t)))
      self.assertFalse(self.safety.toyota_tx_hook(self._torque_msg(sign * 380)))

      self._set_prev_torque(0)
      for t in np.arange(0, 370, 10):
        t *= sign
        self.safety.set_torque_meas(t, t)
        self.assertTrue(self.safety.toyota_tx_hook(self._torque_msg(t)))

      # Increase timer to update rt_torque_last
      self.safety.set_timer(RT_INTERVAL + 1)
      self.assertTrue(self.safety.toyota_tx_hook(self._torque_msg(sign * 370)))
      self.assertTrue(self.safety.toyota_tx_hook(self._torque_msg(sign * 380)))

  def test_torque_measurements(self):
    self.safety.toyota_rx_hook(self._torque_meas_msg(50))
    self.safety.toyota_rx_hook(self._torque_meas_msg(-50))
    self.safety.toyota_rx_hook(self._torque_meas_msg(0))

    self.assertEqual(-51, self.safety.get_torque_meas_min())
    self.assertEqual(51, self.safety.get_torque_meas_max())

    self.safety.toyota_rx_hook(self._torque_meas_msg(0))
    self.assertEqual(-1, self.safety.get_torque_meas_max())
    self.assertEqual(-51, self.safety.get_torque_meas_min())

    self.safety.toyota_rx_hook(self._torque_meas_msg(0))
    self.assertEqual(-1, self.safety.get_torque_meas_max())
    self.assertEqual(-1, self.safety.get_torque_meas_min())


if __name__ == "__main__":
  unittest.main()
