#!/usr/bin/env python3
import unittest
import numpy as np
from panda import Panda
from panda.tests.safety import libpandasafety_py
import panda.tests.safety.common as common
from panda.tests.safety.common import CANPackerPanda, MAX_WRONG_COUNTERS

MAX_RATE_UP = 3
MAX_RATE_DOWN = 3
MAX_STEER = 300
MAX_RT_DELTA = 56
RT_INTERVAL = 250000

DRIVER_TORQUE_ALLOWANCE = 25
DRIVER_TORQUE_FACTOR = 3

MSG_ECM_1 = 0x92              # RX from ECM, for gas pedal
MSG_ABS_1 = 0xC0              # RX from ABS, for wheel speeds
MSG_ABS_2 = 0xC2              # RX from ABS, for wheel speeds and braking
MSG_ACC = 0x110               # RX from ACC, for ACC engagement state
MSG_LKAS = 0x112              # TX from openpilot, for LKAS torque
MSG_EPS_2 = 0x150             # RX from EPS, torque inputs and outputs


class TestFawSafety(common.PandaSafetyTest):
  cnt_ecm_1 = 0
  cnt_abs_1 = 0
  cnt_abs_2 = 0
  cnt_acc = 0
  cnt_lkas = 0
  cnt_eps_2 = 0

  STANDSTILL_THRESHOLD = 1
  RELAY_MALFUNCTION_ADDR = MSG_LKAS
  RELAY_MALFUNCTION_BUS = 0

  @classmethod
  def setUpClass(cls):
    if cls.__name__ == "TestFawSafety":
      cls.packer = None
      cls.safety = None
      raise unittest.SkipTest

  def _set_prev_torque(self, t):
    self.safety.set_desired_torque_last(t)
    self.safety.set_rt_torque_last(t)

  # Wheel speeds
  def _speed_msg(self, speed):
    values = {"FRONT_LEFT": speed, "FRONT_RIGHT": speed, "COUNTER": self.cnt_abs_1 % 16}
    self.__class__.cnt_abs_1 += 1
    return self.packer.make_can_msg_panda("ABS_1", 0, values)

  # Brake pressed
  def _brake_msg(self, brake):
    values = {"BRAKE_PRESSURE": brake, "COUNTER": self.cnt_abs_2 % 16}
    self.__class__.cnt_abs_2 += 1
    return self.packer.make_can_msg_panda("ABS_2", 0, values)

  # Driver throttle input
  def _gas_msg(self, gas):
    values = {"DRIVER_THROTTLE": gas, "COUNTER": self.cnt_ecm_1 % 16}
    self.__class__.cnt_ecm_1 += 1
    return self.packer.make_can_msg_panda("ECM_1", 0, values)

  # ACC engagement status
  def _pcm_status_msg(self, enable):
    values = {"STATUS": 20 if enable else 11, "COUNTER": self.cnt_acc % 16}
    self.__class__.cnt_acc += 1
    return self.packer.make_can_msg_panda("ACC", 2, values)

  # Driver steering input torque
  def _eps_2_msg(self, torque):
    values = {"DRIVER_INPUT_TORQUE": abs(torque), "EPS_TORQUE_DIRECTION": torque < 0,
              "COUNTER": self.cnt_eps_2 % 16}
    self.__class__.cnt_eps_2 += 1
    return self.packer.make_can_msg_panda("EPS_2", 0, values)

  # openpilot steering output torque
  def _lkas_msg(self, torque):
    values = {"LKAS_TORQUE": abs(torque), "LKAS_TORQUE_DIRECTION": torque < 0,
              "COUNTER": self.cnt_lkas % 16}
    self.__class__.cnt_lkas += 1
    return self.packer.make_can_msg_panda("LKAS", 0, values)

  # Cruise control buttons
  # TODO: implement this
  #def _gra_acc_01_msg(self, cancel=0, resume=0, _set=0):
  #  values = {"GRA_Abbrechen": cancel, "GRA_Tip_Setzen": _set,
  #            "GRA_Tip_Wiederaufnahme": resume, "COUNTER": self.cnt_gra_acc_01 % 16}
  #  self.__class__.cnt_gra_acc_01 += 1
  #  return self.packer.make_can_msg_panda("GRA_ACC_01", 0, values)

  def test_steer_safety_check(self):
    for enabled in [0, 1]:
      for t in range(-500, 500):
        self.safety.set_controls_allowed(enabled)
        self._set_prev_torque(t)
        if abs(t) > MAX_STEER or (not enabled and abs(t) > 0):
          self.assertFalse(self._tx(self._lkas_msg(t)))
        else:
          self.assertTrue(self._tx(self._lkas_msg(t)))

  def test_non_realtime_limit_up(self):
    self.safety.set_torque_driver(0, 0)
    self.safety.set_controls_allowed(True)

    self._set_prev_torque(0)
    self.assertTrue(self._tx(self._lkas_msg(MAX_RATE_UP)))
    self._set_prev_torque(0)
    self.assertTrue(self._tx(self._lkas_msg(-MAX_RATE_UP)))

    self._set_prev_torque(0)
    self.assertFalse(self._tx(self._lkas_msg(MAX_RATE_UP + 1)))
    self.safety.set_controls_allowed(True)
    self._set_prev_torque(0)
    self.assertFalse(self._tx(self._lkas_msg(-MAX_RATE_UP - 1)))

  def test_non_realtime_limit_down(self):
    self.safety.set_torque_driver(0, 0)
    self.safety.set_controls_allowed(True)

  def test_against_torque_driver(self):
    self.safety.set_controls_allowed(True)

    for sign in [-1, 1]:
      for t in np.arange(0, DRIVER_TORQUE_ALLOWANCE + 1, 1):
        t *= -sign
        self.safety.set_torque_driver(t, t)
        self._set_prev_torque(MAX_STEER * sign)
        self.assertTrue(self._tx(self._lkas_msg(MAX_STEER * sign)))

      self.safety.set_torque_driver(DRIVER_TORQUE_ALLOWANCE + 1, DRIVER_TORQUE_ALLOWANCE + 1)
      self.assertFalse(self._tx(self._lkas_msg(-MAX_STEER)))

    # spot check some individual cases
    for sign in [-1, 1]:
      driver_torque = (DRIVER_TORQUE_ALLOWANCE + 10) * sign
      torque_desired = (MAX_STEER - 10 * DRIVER_TORQUE_FACTOR) * sign
      delta = 1 * sign
      self._set_prev_torque(torque_desired)
      self.safety.set_torque_driver(-driver_torque, -driver_torque)
      self.assertTrue(self._tx(self._lkas_msg(torque_desired)))
      self._set_prev_torque(torque_desired + delta)
      self.safety.set_torque_driver(-driver_torque, -driver_torque)
      self.assertFalse(self._tx(self._lkas_msg(torque_desired + delta)))

      self._set_prev_torque(MAX_STEER * sign)
      self.safety.set_torque_driver(-MAX_STEER * sign, -MAX_STEER * sign)
      self.assertTrue(self._tx(self._lkas_msg((MAX_STEER - MAX_RATE_DOWN) * sign)))
      self._set_prev_torque(MAX_STEER * sign)
      self.safety.set_torque_driver(-MAX_STEER * sign, -MAX_STEER * sign)
      self.assertTrue(self._tx(self._lkas_msg(0)))
      self._set_prev_torque(MAX_STEER * sign)
      self.safety.set_torque_driver(-MAX_STEER * sign, -MAX_STEER * sign)
      self.assertFalse(self._tx(self._lkas_msg((MAX_STEER - MAX_RATE_DOWN + 1) * sign)))

  def test_realtime_limits(self):
    self.safety.set_controls_allowed(True)

    for sign in [-1, 1]:
      self.safety.init_tests()
      self._set_prev_torque(0)
      self.safety.set_torque_driver(0, 0)
      for t in np.arange(0, MAX_RT_DELTA, 1):
        t *= sign
        self.assertTrue(self._tx(self._lkas_msg(t)))
      self.assertFalse(self._tx(self._lkas_msg(sign * (MAX_RT_DELTA + 1))))

      self._set_prev_torque(0)
      for t in np.arange(0, MAX_RT_DELTA, 1):
        t *= sign
        self.assertTrue(self._tx(self._lkas_msg(t)))

      # Increase timer to update rt_torque_last
      self.safety.set_timer(RT_INTERVAL + 1)
      self.assertTrue(self._tx(self._lkas_msg(sign * (MAX_RT_DELTA - 1))))
      self.assertTrue(self._tx(self._lkas_msg(sign * (MAX_RT_DELTA + 1))))

  def test_torque_measurements(self):
    self._rx(self._eps_2_msg(50))
    self._rx(self._eps_2_msg(-50))
    self._rx(self._eps_2_msg(0))
    self._rx(self._eps_2_msg(0))
    self._rx(self._eps_2_msg(0))
    self._rx(self._eps_2_msg(0))

    self.assertEqual(-50, self.safety.get_torque_driver_min())
    self.assertEqual(50, self.safety.get_torque_driver_max())

    self._rx(self._eps_2_msg(0))
    self.assertEqual(0, self.safety.get_torque_driver_max())
    self.assertEqual(-50, self.safety.get_torque_driver_min())

    self._rx(self._eps_2_msg(0))
    self.assertEqual(0, self.safety.get_torque_driver_max())
    self.assertEqual(0, self.safety.get_torque_driver_min())

  def test_rx_hook(self):
    # checksum checks
    for msg in [MSG_EPS_2, MSG_ABS_2, MSG_ACC, MSG_ECM_1, MSG_ABS_1]:
      self.safety.set_controls_allowed(1)
      if msg == MSG_EPS_2:
        to_push = self._eps_2_msg(0)
      if msg == MSG_ABS_2:
        to_push = self._brake_msg(False)
      if msg == MSG_ACC:
        to_push = self._pcm_status_msg(True)
      if msg == MSG_ECM_1:
        to_push = self._gas_msg(0)
      if msg == MSG_ABS_1:
        to_push = self._speed_msg(0)
      self.assertTrue(self._rx(to_push))
      to_push[0].data[4] ^= 0xFF
      self.assertFalse(self._rx(to_push))
      self.assertFalse(self.safety.get_controls_allowed())

    # counter
    # reset wrong_counters to zero by sending valid messages
    for i in range(MAX_WRONG_COUNTERS + 1):
      self.__class__.cnt_eps_2 += 1
      self.__class__.cnt_abs_2 += 1
      self.__class__.cnt_acc += 1
      self.__class__.cnt_ecm_1 += 1
      self.__class__.cnt_abs_1 += 1
      if i < MAX_WRONG_COUNTERS:
        self.safety.set_controls_allowed(1)
        self._rx(self._eps_2_msg(0))
        self._rx(self._brake_msg(False))
        self._rx(self._pcm_status_msg(True))
        self._rx(self._gas_msg(0))
        self._rx(self._speed_msg(0))
      else:
        self.assertFalse(self._rx(self._eps_2_msg(0)))
        self.assertFalse(self._rx(self._brake_msg(False)))
        self.assertFalse(self._rx(self._pcm_status_msg(True)))
        self.assertFalse(self._rx(self._gas_msg(0)))
        self.assertFalse(self._rx(self._speed_msg(0)))
        self.assertFalse(self.safety.get_controls_allowed())

    # restore counters for future tests with a couple of good messages
    for i in range(2):
      self.safety.set_controls_allowed(1)
      self._rx(self._eps_2_msg(0))
      self._rx(self._brake_msg(False))
      self._rx(self._pcm_status_msg(True))
      self._rx(self._gas_msg(0))
      self._rx(self._speed_msg(0))
    self.assertTrue(self.safety.get_controls_allowed())


class TestFawStockSafety(TestFawSafety):
  TX_MSGS = [[MSG_LKAS, 0]]
  FWD_BLACKLISTED_ADDRS = {2: [MSG_LKAS]}
  FWD_BUS_LOOKUP = {0: 2, 2: 0}

  def setUp(self):
    self.packer = CANPackerPanda("faw")
    self.safety = libpandasafety_py.libpandasafety
    self.safety.set_safety_hooks(Panda.SAFETY_FAW, 0)
    self.safety.init_tests()

  # TODO: implement
  #def test_spam_cancel_safety_check(self):
  #  self.safety.set_controls_allowed(0)
  #  self.assertTrue(self._tx(self._gra_acc_01_msg(cancel=1)))
  #  self.assertFalse(self._tx(self._gra_acc_01_msg(resume=1)))
  #  self.assertFalse(self._tx(self._gra_acc_01_msg(_set=1)))
  #  # do not block resume if we are engaged already
  #  self.safety.set_controls_allowed(1)
  #  self.assertTrue(self._tx(self._gra_acc_01_msg(resume=1)))


if __name__ == "__main__":
  unittest.main()
