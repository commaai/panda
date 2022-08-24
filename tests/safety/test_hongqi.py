#!/usr/bin/env python3
import unittest
from panda import Panda
from panda.tests.safety import libpandasafety_py
import panda.tests.safety.common as common
from panda.tests.safety.common import CANPackerPanda

MSG_ECM_1 = 0x92              # RX from ECM, for gas pedal
MSG_ABS_1 = 0xC0              # RX from ABS, for wheel speeds
MSG_MAYBE_ABS = 0x94          # RX from ABS? for brake pressed state
MSG_ACC = 0x110               # RX from ACC, for ACC engagement state
MSG_LKAS = 0x112              # TX from openpilot, for LKAS torque
MSG_EPS_2 = 0x150             # RX from EPS, torque inputs and outputs


class TestHongqiSafety(common.PandaSafetyTest, common.DriverTorqueSteeringSafetyTest):
  STANDSTILL_THRESHOLD = 1
  RELAY_MALFUNCTION_ADDR = MSG_LKAS
  RELAY_MALFUNCTION_BUS = 0

  MAX_RATE_UP = 6
  MAX_RATE_DOWN = 10
  MAX_TORQUE = 300
  MAX_RT_DELTA = 113
  RT_INTERVAL = 250000

  DRIVER_TORQUE_ALLOWANCE = 50
  DRIVER_TORQUE_FACTOR = 3

  @classmethod
  def setUpClass(cls):
    if cls.__name__ == "TestHongqiSafety":
      cls.packer = None
      cls.safety = None
      raise unittest.SkipTest

  # Wheel speeds
  def _speed_msg(self, speed):
    values = {"FRONT_LEFT": speed, "FRONT_RIGHT": speed}
    return self.packer.make_can_msg_panda("ABS_1", 0, values)

  # Brake pressed
  def _user_brake_msg(self, brake):
    values = {"BRAKE_PRESSED": brake}
    return self.packer.make_can_msg_panda("MAYBE_ABS", 0, values)

  # Driver throttle input
  def _user_gas_msg(self, gas):
    values = {"DRIVER_THROTTLE": gas}
    return self.packer.make_can_msg_panda("ECM_1", 0, values)

  # ACC engagement status
  def _pcm_status_msg(self, enable):
    values = {"STATUS": 5 if enable else 2}
    return self.packer.make_can_msg_panda("ACC", 2, values)

  # Driver steering input torque
  def _torque_driver_msg(self, torque):
    values = {"DRIVER_INPUT_TORQUE": abs(torque), "EPS_TORQUE_DIRECTION": torque < 0}
    return self.packer.make_can_msg_panda("EPS_2", 0, values)

  # openpilot steering output torque
  def _torque_cmd_msg(self, torque, steer_req=1):
    values = {"LKAS_TORQUE": abs(torque), "LKAS_TORQUE_DIRECTION": torque < 0}
    return self.packer.make_can_msg_panda("LKAS", 0, values)

  # Cruise control buttons
  # TODO: implement this
  #def _gra_acc_01_msg(self, cancel=0, resume=0, _set=0):
  #  values = {"GRA_Abbrechen": cancel, "GRA_Tip_Setzen": _set,
  #            "GRA_Tip_Wiederaufnahme": resume, "COUNTER": self.cnt_gra_acc_01 % 16}
  #  self.__class__.cnt_gra_acc_01 += 1
  #  return self.packer.make_can_msg_panda("GRA_ACC_01", 0, values)

  def test_torque_measurements(self):
    self._rx(self._torque_driver_msg(50))
    self._rx(self._torque_driver_msg(-50))
    self._rx(self._torque_driver_msg(0))
    self._rx(self._torque_driver_msg(0))
    self._rx(self._torque_driver_msg(0))
    self._rx(self._torque_driver_msg(0))

    self.assertEqual(-50, self.safety.get_torque_driver_min())
    self.assertEqual(50, self.safety.get_torque_driver_max())

    self._rx(self._torque_driver_msg(0))
    self.assertEqual(0, self.safety.get_torque_driver_max())
    self.assertEqual(-50, self.safety.get_torque_driver_min())

    self._rx(self._torque_driver_msg(0))
    self.assertEqual(0, self.safety.get_torque_driver_max())
    self.assertEqual(0, self.safety.get_torque_driver_min())


class TestHongqiStockSafety(TestHongqiSafety):
  TX_MSGS = [[MSG_LKAS, 0]]
  FWD_BLACKLISTED_ADDRS = {2: [MSG_LKAS]}
  FWD_BUS_LOOKUP = {0: 2, 2: 0}

  def setUp(self):
    self.packer = CANPackerPanda("hongqi_hs5")
    self.safety = libpandasafety_py.libpandasafety
    self.safety.set_safety_hooks(Panda.SAFETY_HONGQI, 0)
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
