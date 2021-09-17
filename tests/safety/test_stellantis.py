#!/usr/bin/env python3
import unittest
import numpy as np
from panda import Panda
from panda.tests.safety import libpandasafety_py
import panda.tests.safety.common as common
from panda.tests.safety.common import CANPackerPanda, MAX_WRONG_COUNTERS

# TODO: Limits and rates and tolerances are unverified/untested
MAX_RATE_UP = 5
MAX_RATE_DOWN = 10
MAX_STEER = 255
MAX_RT_DELTA = 93
RT_INTERVAL = 250000

DRIVER_TORQUE_ALLOWANCE = 80
DRIVER_TORQUE_FACTOR = 3

MSG_EPS_2 = 0x31            # EPS driver input torque
MSG_ABS_1 = 0x79            # Brake pedal and pressure
MSG_TPS_1 = 0x81            # Throttle position sensor
MSG_ABS_4 = 0x8B            # ABS wheel speeds
MSG_DASM_ACC_CMD_1 = 0x99   # ACC engagement states from DASM
MSG_DASM_LKAS_CMD = 0xA6    # LKAS controls from DASM
MSG_CSWC = 0xB1             # Cruise control buttons
MSG_DASM_LKAS_HUD = 0xFA    # LKAS HUD and auto headlight control from DASM

class TestStellantisSafety(common.PandaSafetyTest):
  cnt_eps_2 = 0
  cnt_abs_1 = 0
  cnt_dasm_acc_cmd_1 = 0
  cnt_dasm_lkas_cmd = 0
  cnt_cswc = 0

  TX_MSGS = [[MSG_DASM_LKAS_CMD, 0], [MSG_DASM_LKAS_HUD, 0], [MSG_CSWC, 2]]
  STANDSTILL_THRESHOLD = 0.3
  GAS_PRESSED_THRESHOLD = 20
  RELAY_MALFUNCTION_ADDR = MSG_DASM_LKAS_CMD
  RELAY_MALFUNCTION_BUS = 0
  FWD_BLACKLISTED_ADDRS = {2: [MSG_DASM_LKAS_CMD, MSG_DASM_LKAS_HUD]}
  FWD_BUS_LOOKUP = {0: 2, 2: 0}

  def setUp(self):
    self.packer = CANPackerPanda("stellantis_dasm")
    self.safety = libpandasafety_py.libpandasafety
    self.safety.set_safety_hooks(Panda.SAFETY_STELLANTIS, 0)
    self.safety.init_tests()

  def _set_prev_torque(self, t):
    self.safety.set_desired_torque_last(t)
    self.safety.set_rt_torque_last(t)

  # Wheel speeds
  def _speed_msg(self, speed):
    values = {"WHEEL_SPEED_%s" % s: speed for s in ["FL", "FR", "RL", "RR"]}
    return self.packer.make_can_msg_panda("ABS_4", 0, values)

  # Brake pedal
  def _brake_msg(self, brake):
    values = {"BRAKE_PEDAL": brake, "COUNTER": self.cnt_abs_1 % 16}
    self.__class__.cnt_abs_1 += 1
    return self.packer.make_can_msg_panda("ABS_1", 0, values)

  # Driver throttle input
  def _gas_msg(self, gas):
    values = {"THROTTLE_POSITION": gas}
    return self.packer.make_can_msg_panda("TPS_1", 0, values)

  # ACC engagement status
  def _pcm_status_msg(self, enable):
    values = {"ACC_STATUS": 3 if enable else 1, "COUNTER": self.cnt_dasm_acc_cmd_1 % 16}
    self.__class__.cnt_dasm_acc_cmd_1 += 1
    return self.packer.make_can_msg_panda("DASM_ACC_CMD_1", 2, values)

  # Driver steering input torque
  def _eps_msg(self, torque):
    values = {"TORQUE_DRIVER": torque, "COUNTER": self.cnt_eps_2 % 16}
    self.__class__.cnt_eps_2 += 1
    return self.packer.make_can_msg_panda("EPS_2", 0, values)

  # openpilot steering output torque
  def _torque_msg(self, torque):
    values = {"LKAS_COMMAND": torque, "COUNTER": self.cnt_dasm_lkas_cmd % 16}
    self.__class__.cnt_dasm_lkas_cmd += 1
    return self.packer.make_can_msg_panda("DASM_LKAS_CMD", 0, values)

  # Cruise control buttons
  def _acc_buttons_msg(self, cancel=0, resume=0, _set=0):
    values = {"CANCEL": cancel, "SET_PLUS": _set,
              "RESUME": resume, "COUNTER": self.cnt_cswc % 16}
    self.__class__.cnt_cswc += 1
    return self.packer.make_can_msg_panda("CSWC", 2, values)

  def test_spam_cancel_safety_check(self):
    self.safety.set_controls_allowed(0)
    self.assertTrue(self._tx(self._acc_buttons_msg(cancel=1)))
    self.assertFalse(self._tx(self._acc_buttons_msg(resume=1)))
    self.assertFalse(self._tx(self._acc_buttons_msg(_set=1)))
    # do not block resume if we are engaged already
    self.safety.set_controls_allowed(1)
    self.assertTrue(self._tx(self._acc_buttons_msg(resume=1)))

  def test_non_realtime_limit_up(self):
    self.safety.set_torque_driver(0, 0)
    self.safety.set_controls_allowed(True)

    self._set_prev_torque(0)
    self.assertTrue(self._tx(self._torque_msg(MAX_RATE_UP)))
    self._set_prev_torque(0)
    self.assertTrue(self._tx(self._torque_msg(-MAX_RATE_UP)))

    self._set_prev_torque(0)
    self.assertFalse(self._tx(self._torque_msg(MAX_RATE_UP + 1)))
    self.safety.set_controls_allowed(True)
    self._set_prev_torque(0)
    self.assertFalse(self._tx(self._torque_msg(-MAX_RATE_UP - 1)))

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
        self.assertTrue(self._tx(self._torque_msg(MAX_STEER * sign)))

      self.safety.set_torque_driver(DRIVER_TORQUE_ALLOWANCE + 1, DRIVER_TORQUE_ALLOWANCE + 1)
      self.assertFalse(self._tx(self._torque_msg(-MAX_STEER)))

    # spot check some individual cases
    for sign in [-1, 1]:
      driver_torque = (DRIVER_TORQUE_ALLOWANCE + 10) * sign
      torque_desired = (MAX_STEER - 10 * DRIVER_TORQUE_FACTOR) * sign
      delta = 1 * sign
      self._set_prev_torque(torque_desired)
      self.safety.set_torque_driver(-driver_torque, -driver_torque)
      self.assertTrue(self._tx(self._torque_msg(torque_desired)))
      self._set_prev_torque(torque_desired + delta)
      self.safety.set_torque_driver(-driver_torque, -driver_torque)
      self.assertFalse(self._tx(self._torque_msg(torque_desired + delta)))

      self._set_prev_torque(MAX_STEER * sign)
      self.safety.set_torque_driver(-MAX_STEER * sign, -MAX_STEER * sign)
      self.assertTrue(self._tx(self._torque_msg((MAX_STEER - MAX_RATE_DOWN) * sign)))
      self._set_prev_torque(MAX_STEER * sign)
      self.safety.set_torque_driver(-MAX_STEER * sign, -MAX_STEER * sign)
      self.assertTrue(self._tx(self._torque_msg(0)))
      self._set_prev_torque(MAX_STEER * sign)
      self.safety.set_torque_driver(-MAX_STEER * sign, -MAX_STEER * sign)
      self.assertFalse(self._tx(self._torque_msg((MAX_STEER - MAX_RATE_DOWN + 1) * sign)))

  def test_realtime_limits(self):
    self.safety.set_controls_allowed(True)

    for sign in [-1, 1]:
      self.safety.init_tests()
      self._set_prev_torque(0)
      self.safety.set_torque_driver(0, 0)
      for t in np.arange(0, MAX_RT_DELTA, 1):
        t *= sign
        self.assertTrue(self._tx(self._torque_msg(t)))
      self.assertFalse(self._tx(self._torque_msg(sign * (MAX_RT_DELTA + 1))))

      self._set_prev_torque(0)
      for t in np.arange(0, MAX_RT_DELTA, 1):
        t *= sign
        self.assertTrue(self._tx(self._torque_msg(t)))

      # Increase timer to update rt_torque_last
      self.safety.set_timer(RT_INTERVAL + 1)
      self.assertTrue(self._tx(self._torque_msg(sign * (MAX_RT_DELTA - 1))))
      self.assertTrue(self._tx(self._torque_msg(sign * (MAX_RT_DELTA + 1))))

  def test_torque_measurements(self):
    self.safety.set_torque_driver(0, 0)
    self.safety.set_controls_allowed(True)

    self._rx(self._eps_msg(50))
    self._rx(self._eps_msg(-50))
    self._rx(self._eps_msg(0))
    self._rx(self._eps_msg(0))
    self._rx(self._eps_msg(0))
    self._rx(self._eps_msg(0))

    self.assertEqual(-50, self.safety.get_torque_driver_min())
    self.assertEqual(50, self.safety.get_torque_driver_max())

    self._rx(self._eps_msg(0))
    self.assertEqual(0, self.safety.get_torque_driver_max())
    self.assertEqual(-50, self.safety.get_torque_driver_min())

    self._rx(self._eps_msg(0))
    self.assertEqual(0, self.safety.get_torque_driver_max())
    self.assertEqual(0, self.safety.get_torque_driver_min())

  def test_rx_hook(self):
    # checksum checks
    for msg in [MSG_EPS_2, MSG_ABS_1, MSG_DASM_ACC_CMD_1]:
      self.safety.set_controls_allowed(1)
      if msg == MSG_EPS_2:
        to_push = self._eps_msg(0)
      if msg == MSG_ABS_1:
        to_push = self._brake_msg(False)
      if msg == MSG_DASM_ACC_CMD_1:
        to_push = self._pcm_status_msg(True)
      self.assertTrue(self._rx(to_push))
      to_push[0].RDHR ^= 0xFF
      self.assertFalse(self._rx(to_push))
      self.assertFalse(self.safety.get_controls_allowed())

    # counter
    # reset wrong_counters to zero by sending valid messages
    for i in range(MAX_WRONG_COUNTERS + 1):
      self.__class__.cnt_eps_2 += 1
      self.__class__.cnt_abs_1 += 1
      self.__class__.cnt_dasm_acc_cmd_1 += 1
      if i < MAX_WRONG_COUNTERS:
        self.safety.set_controls_allowed(1)
        self._rx(self._eps_msg(0))
        self._rx(self._brake_msg(False))
        self._rx(self._pcm_status_msg(True))
      else:
        self.assertFalse(self._rx(self._eps_msg(0)))
        self.assertFalse(self._rx(self._brake_msg(False)))
        self.assertFalse(self._rx(self._pcm_status_msg(True)))
        self.assertFalse(self.safety.get_controls_allowed())

    # restore counters for future tests with a couple of good messages
    for i in range(2):
      self.safety.set_controls_allowed(1)
      self._rx(self._eps_msg(0))
      self._rx(self._brake_msg(False))
      self._rx(self._pcm_status_msg(True))
    self.assertTrue(self.safety.get_controls_allowed())


if __name__ == "__main__":
  unittest.main()
