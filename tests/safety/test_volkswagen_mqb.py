#!/usr/bin/env python3
import unittest
from panda import Panda
from panda.tests.safety import libpandasafety_py
import panda.tests.safety.common as common
from panda.tests.safety.common import CANPackerPanda

MSG_ESP_19 = 0xB2       # RX from ABS, for wheel speeds
MSG_LH_EPS_03 = 0x9F    # RX from EPS, for driver steering torque
MSG_ESP_05 = 0x106      # RX from ABS, for brake light state
MSG_TSK_06 = 0x120      # RX from ECU, for ACC status from drivetrain coordinator
MSG_MOTOR_20 = 0x121    # RX from ECU, for driver throttle input
MSG_HCA_01 = 0x126      # TX by OP, Heading Control Assist steering torque
MSG_GRA_ACC_01 = 0x12B  # TX by OP, ACC control buttons for cancel/resume
MSG_LDW_02 = 0x397      # TX by OP, Lane line recognition and text alerts


class TestVolkswagenMqbSafety(common.PandaSafetyTest, common.DriverTorqueSteeringSafetyTest):
  STANDSTILL_THRESHOLD = 1
  RELAY_MALFUNCTION_ADDR = MSG_HCA_01
  RELAY_MALFUNCTION_BUS = 0

  MAX_RATE_UP = 4
  MAX_RATE_DOWN = 10
  MAX_TORQUE = 300
  MAX_RT_DELTA = 75
  RT_INTERVAL = 250000

  DRIVER_TORQUE_ALLOWANCE = 80
  DRIVER_TORQUE_FACTOR = 3

  @classmethod
  def setUpClass(cls):
    if cls.__name__ == "TestVolkswagenMqbSafety":
      cls.packer = None
      cls.safety = None
      raise unittest.SkipTest

  # Wheel speeds _esp_19_msg
  def _speed_msg(self, speed):
    values = {"ESP_%s_Radgeschw_02" % s: speed for s in ["HL", "HR", "VL", "VR"]}
    return self.packer.make_can_msg_panda("ESP_19", 0, values)

  # Brake light switch _esp_05_msg
  def _user_brake_msg(self, brake):
    values = {"ESP_Fahrer_bremst": brake}
    return self.packer.make_can_msg_panda("ESP_05", 0, values)

  # Driver throttle input
  def _user_gas_msg(self, gas):
    values = {"MO_Fahrpedalrohwert_01": gas}
    return self.packer.make_can_msg_panda("Motor_20", 0, values)

  # ACC engagement status
  def _pcm_status_msg(self, enable):
    values = {"TSK_Status": 3 if enable else 1}
    return self.packer.make_can_msg_panda("TSK_06", 0, values)

  # Driver steering input torque
  def _torque_driver_msg(self, torque):
    values = {"EPS_Lenkmoment": abs(torque), "EPS_VZ_Lenkmoment": torque < 0}
    return self.packer.make_can_msg_panda("LH_EPS_03", 0, values)

  # openpilot steering output torque
  def _torque_cmd_msg(self, torque, steer_req=1):
    values = {"Assist_Torque": abs(torque), "Assist_VZ": torque < 0}
    return self.packer.make_can_msg_panda("HCA_01", 0, values)

  # Cruise control buttons
  def _gra_acc_01_msg(self, cancel=0, resume=0, _set=0):
    values = {"GRA_Abbrechen": cancel, "GRA_Tip_Setzen": _set, "GRA_Tip_Wiederaufnahme": resume}
    return self.packer.make_can_msg_panda("GRA_ACC_01", 0, values)

  def test_torque_measurements(self):
    # TODO: make this test work with all cars
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


class TestVolkswagenMqbStockSafety(TestVolkswagenMqbSafety):
  TX_MSGS = [[MSG_HCA_01, 0], [MSG_LDW_02, 0], [MSG_GRA_ACC_01, 0], [MSG_GRA_ACC_01, 2]]
  FWD_BLACKLISTED_ADDRS = {2: [MSG_HCA_01, MSG_LDW_02]}
  FWD_BUS_LOOKUP = {0: 2, 2: 0}

  def setUp(self):
    self.packer = CANPackerPanda("vw_mqb_2010")
    self.safety = libpandasafety_py.libpandasafety
    self.safety.set_safety_hooks(Panda.SAFETY_VOLKSWAGEN_MQB, 0)
    self.safety.init_tests()

  def test_spam_cancel_safety_check(self):
    self.safety.set_controls_allowed(0)
    self.assertTrue(self._tx(self._gra_acc_01_msg(cancel=1)))
    self.assertFalse(self._tx(self._gra_acc_01_msg(resume=1)))
    self.assertFalse(self._tx(self._gra_acc_01_msg(_set=1)))
    # do not block resume if we are engaged already
    self.safety.set_controls_allowed(1)
    self.assertTrue(self._tx(self._gra_acc_01_msg(resume=1)))


if __name__ == "__main__":
  unittest.main()
