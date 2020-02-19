#!/usr/bin/env python3
import unittest
import numpy as np
from panda import Panda
from panda.tests.safety import libpandasafety_py
from panda.tests.safety.common import test_relay_malfunction, make_msg, \
  test_manually_enable_controls_allowed, test_spam_can_buses

MAX_RATE_UP = 4
MAX_RATE_DOWN = 10
MAX_STEER = 250
MAX_RT_DELTA = 75
RT_INTERVAL = 250000

DRIVER_TORQUE_ALLOWANCE = 80
DRIVER_TORQUE_FACTOR = 3

MSG_EPS_01 = 0x9F       # RX from EPS, for driver steering torque
MSG_ACC_06 = 0x122      # RX from ACC radar, for status and engagement
MSG_MOTOR_20 = 0x121    # RX from ECU, for driver throttle input
MSG_HCA_01 = 0x126      # TX by OP, Heading Control Assist steering torque
MSG_GRA_ACC_01 = 0x12B  # TX by OP, ACC control buttons for cancel/resume
MSG_LDW_02 = 0x397      # TX by OP, Lane line recognition and text alerts

# Transmit of GRA_ACC_01 is allowed on bus 0 and 2 to keep compatibility with gateway and camera integration
TX_MSGS = [[MSG_HCA_01, 0], [MSG_GRA_ACC_01, 0], [MSG_GRA_ACC_01, 2], [MSG_LDW_02, 0]]

def sign(a):
  if a > 0:
    return 1
  else:
    return -1

class TestVolkswagenMqbSafety(unittest.TestCase):
  @classmethod
  def setUp(cls):
    cls.safety = libpandasafety_py.libpandasafety
    cls.safety.set_safety_hooks(Panda.SAFETY_VOLKSWAGEN_MQB, 0)
    cls.safety.init_tests_volkswagen()

  def _set_prev_torque(self, t):
    self.safety.set_volkswagen_desired_torque_last(t)
    self.safety.set_volkswagen_rt_torque_last(t)

  # Driver steering input torque
  def _eps_01_msg(self, torque):
    to_send = make_msg(0, MSG_EPS_01)
    t = abs(torque)
    to_send[0].RDHR = ((t & 0x1FFF) << 8)
    if torque < 0:
      to_send[0].RDHR |= 0x1 << 23
    return to_send

  # openpilot steering output torque
  def _hca_01_msg(self, torque):
    to_send = make_msg(0, MSG_HCA_01)
    t = abs(torque)
    to_send[0].RDLR = (t & 0xFFF) << 16
    if torque < 0:
      to_send[0].RDLR |= 0x1 << 31
    return to_send

  # ACC engagement status
  def _acc_06_msg(self, status):
    to_send = make_msg(0, MSG_ACC_06)
    to_send[0].RDHR = (status & 0x7) << 28
    return to_send

  # Driver throttle input
  def _motor_20_msg(self, gas):
    to_send = make_msg(0, MSG_MOTOR_20)
    to_send[0].RDLR = (gas & 0xFF) << 12
    return to_send

  # Cruise control buttons
  def _gra_acc_01_msg(self, bit):
    to_send = make_msg(2, MSG_GRA_ACC_01)
    to_send[0].RDLR = 1 << bit
    return to_send

  def test_spam_can_buses(self):
    test_spam_can_buses(self, TX_MSGS)

  def test_relay_malfunction(self):
    test_relay_malfunction(self, MSG_HCA_01)

  def test_prev_gas(self):
    for g in range(0, 256):
      self.safety.safety_rx_hook(self._motor_20_msg(g))
      self.assertEqual(g, self.safety.get_volkswagen_gas_prev())

  def test_default_controls_not_allowed(self):
    self.assertFalse(self.safety.get_controls_allowed())

  def test_enable_control_allowed_from_cruise(self):
    self.safety.set_controls_allowed(0)
    self.safety.safety_rx_hook(self._acc_06_msg(3))
    self.assertTrue(self.safety.get_controls_allowed())

  def test_disable_control_allowed_from_cruise(self):
    self.safety.set_controls_allowed(1)
    self.safety.safety_rx_hook(self._acc_06_msg(1))
    self.assertFalse(self.safety.get_controls_allowed())

  def test_disengage_on_gas(self):
    self.safety.safety_rx_hook(self._motor_20_msg(0))
    self.safety.set_controls_allowed(True)
    self.safety.safety_rx_hook(self._motor_20_msg(1))
    self.assertFalse(self.safety.get_controls_allowed())

  def test_allow_engage_with_gas_pressed(self):
    self.safety.safety_rx_hook(self._motor_20_msg(1))
    self.safety.set_controls_allowed(True)
    self.safety.safety_rx_hook(self._motor_20_msg(1))
    self.assertTrue(self.safety.get_controls_allowed())
    self.safety.safety_rx_hook(self._motor_20_msg(1))
    self.assertTrue(self.safety.get_controls_allowed())

  def test_steer_safety_check(self):
    for enabled in [0, 1]:
      for t in range(-500, 500):
        self.safety.set_controls_allowed(enabled)
        self._set_prev_torque(t)
        if abs(t) > MAX_STEER or (not enabled and abs(t) > 0):
          self.assertFalse(self.safety.safety_tx_hook(self._hca_01_msg(t)))
        else:
          self.assertTrue(self.safety.safety_tx_hook(self._hca_01_msg(t)))

  def test_manually_enable_controls_allowed(self):
    test_manually_enable_controls_allowed(self)

  def test_spam_cancel_safety_check(self):
    BIT_CANCEL = 13
    BIT_RESUME = 19
    BIT_SET = 16
    self.safety.set_controls_allowed(0)
    self.assertTrue(self.safety.safety_tx_hook(self._gra_acc_01_msg(BIT_CANCEL)))
    self.assertFalse(self.safety.safety_tx_hook(self._gra_acc_01_msg(BIT_RESUME)))
    self.assertFalse(self.safety.safety_tx_hook(self._gra_acc_01_msg(BIT_SET)))
    # do not block resume if we are engaged already
    self.safety.set_controls_allowed(1)
    self.assertTrue(self.safety.safety_tx_hook(self._gra_acc_01_msg(BIT_RESUME)))

  def test_non_realtime_limit_up(self):
    self.safety.set_volkswagen_torque_driver(0, 0)
    self.safety.set_controls_allowed(True)

    self._set_prev_torque(0)
    self.assertTrue(self.safety.safety_tx_hook(self._hca_01_msg(MAX_RATE_UP)))
    self._set_prev_torque(0)
    self.assertTrue(self.safety.safety_tx_hook(self._hca_01_msg(-MAX_RATE_UP)))

    self._set_prev_torque(0)
    self.assertFalse(self.safety.safety_tx_hook(self._hca_01_msg(MAX_RATE_UP + 1)))
    self.safety.set_controls_allowed(True)
    self._set_prev_torque(0)
    self.assertFalse(self.safety.safety_tx_hook(self._hca_01_msg(-MAX_RATE_UP - 1)))

  def test_non_realtime_limit_down(self):
    self.safety.set_volkswagen_torque_driver(0, 0)
    self.safety.set_controls_allowed(True)

  def test_against_torque_driver(self):
    self.safety.set_controls_allowed(True)

    for sign in [-1, 1]:
      for t in np.arange(0, DRIVER_TORQUE_ALLOWANCE + 1, 1):
        t *= -sign
        self.safety.set_volkswagen_torque_driver(t, t)
        self._set_prev_torque(MAX_STEER * sign)
        self.assertTrue(self.safety.safety_tx_hook(self._hca_01_msg(MAX_STEER * sign)))

      self.safety.set_volkswagen_torque_driver(DRIVER_TORQUE_ALLOWANCE + 1, DRIVER_TORQUE_ALLOWANCE + 1)
      self.assertFalse(self.safety.safety_tx_hook(self._hca_01_msg(-MAX_STEER)))

    # spot check some individual cases
    for sign in [-1, 1]:
      driver_torque = (DRIVER_TORQUE_ALLOWANCE + 10) * sign
      torque_desired = (MAX_STEER - 10 * DRIVER_TORQUE_FACTOR) * sign
      delta = 1 * sign
      self._set_prev_torque(torque_desired)
      self.safety.set_volkswagen_torque_driver(-driver_torque, -driver_torque)
      self.assertTrue(self.safety.safety_tx_hook(self._hca_01_msg(torque_desired)))
      self._set_prev_torque(torque_desired + delta)
      self.safety.set_volkswagen_torque_driver(-driver_torque, -driver_torque)
      self.assertFalse(self.safety.safety_tx_hook(self._hca_01_msg(torque_desired + delta)))

      self._set_prev_torque(MAX_STEER * sign)
      self.safety.set_volkswagen_torque_driver(-MAX_STEER * sign, -MAX_STEER * sign)
      self.assertTrue(self.safety.safety_tx_hook(self._hca_01_msg((MAX_STEER - MAX_RATE_DOWN) * sign)))
      self._set_prev_torque(MAX_STEER * sign)
      self.safety.set_volkswagen_torque_driver(-MAX_STEER * sign, -MAX_STEER * sign)
      self.assertTrue(self.safety.safety_tx_hook(self._hca_01_msg(0)))
      self._set_prev_torque(MAX_STEER * sign)
      self.safety.set_volkswagen_torque_driver(-MAX_STEER * sign, -MAX_STEER * sign)
      self.assertFalse(self.safety.safety_tx_hook(self._hca_01_msg((MAX_STEER - MAX_RATE_DOWN + 1) * sign)))

  def test_realtime_limits(self):
    self.safety.set_controls_allowed(True)

    for sign in [-1, 1]:
      self.safety.init_tests_volkswagen()
      self._set_prev_torque(0)
      self.safety.set_volkswagen_torque_driver(0, 0)
      for t in np.arange(0, MAX_RT_DELTA, 1):
        t *= sign
        self.assertTrue(self.safety.safety_tx_hook(self._hca_01_msg(t)))
      self.assertFalse(self.safety.safety_tx_hook(self._hca_01_msg(sign * (MAX_RT_DELTA + 1))))

      self._set_prev_torque(0)
      for t in np.arange(0, MAX_RT_DELTA, 1):
        t *= sign
        self.assertTrue(self.safety.safety_tx_hook(self._hca_01_msg(t)))

      # Increase timer to update rt_torque_last
      self.safety.set_timer(RT_INTERVAL + 1)
      self.assertTrue(self.safety.safety_tx_hook(self._hca_01_msg(sign * (MAX_RT_DELTA - 1))))
      self.assertTrue(self.safety.safety_tx_hook(self._hca_01_msg(sign * (MAX_RT_DELTA + 1))))

  def test_torque_measurements(self):
    self.safety.safety_rx_hook(self._eps_01_msg(50))
    self.safety.safety_rx_hook(self._eps_01_msg(-50))
    self.safety.safety_rx_hook(self._eps_01_msg(0))
    self.safety.safety_rx_hook(self._eps_01_msg(0))
    self.safety.safety_rx_hook(self._eps_01_msg(0))
    self.safety.safety_rx_hook(self._eps_01_msg(0))

    self.assertEqual(-50, self.safety.get_volkswagen_torque_driver_min())
    self.assertEqual(50, self.safety.get_volkswagen_torque_driver_max())

    self.safety.safety_rx_hook(self._eps_01_msg(0))
    self.assertEqual(0, self.safety.get_volkswagen_torque_driver_max())
    self.assertEqual(-50, self.safety.get_volkswagen_torque_driver_min())

    self.safety.safety_rx_hook(self._eps_01_msg(0))
    self.assertEqual(0, self.safety.get_volkswagen_torque_driver_max())
    self.assertEqual(0, self.safety.get_volkswagen_torque_driver_min())


  def test_fwd_hook(self):
    buss = list(range(0x0, 0x3))
    msgs = list(range(0x1, 0x800))
    blocked_msgs_0to2 = []
    blocked_msgs_2to0 = [MSG_HCA_01, MSG_LDW_02]
    for b in buss:
      for m in msgs:
        if b == 0:
          fwd_bus = -1 if m in blocked_msgs_0to2 else 2
        elif b == 1:
          fwd_bus = -1
        elif b == 2:
          fwd_bus = -1 if m in blocked_msgs_2to0 else 0

        # assume len 8
        self.assertEqual(fwd_bus, self.safety.safety_fwd_hook(b, make_msg(b, m, 8)))


if __name__ == "__main__":
  unittest.main()
