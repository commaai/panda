#!/usr/bin/env python3
import unittest
from panda import Panda
from panda.tests.libpanda import libpanda_py
import panda.tests.safety.common as common
from panda.tests.safety.common import CANPackerPanda

MSG_LH_EPS_03 = 0x9F      # RX from EPS, for driver steering torque
MSG_GRA_ACC_01 = 0x12B    # TX by OP, ACC control buttons for cancel/resume
MSG_ESP_NEW_1 = 0xFC      # RX from ABS, for wheel speeds
MSG_MOTOR_NEW_1 = 0x10B   # RX from ECU, for ACC status from drivetrain coordinator
MSG_ESP_NEW_3 = 0x139     # RX from ABS, for brake pressure and brake pressed state
MSG_HCA_NEW = 0x303       # TX by OP, steering torque control message


class TestVolkswagenMqbEvoSafety(common.PandaSafetyTest, common.DriverTorqueSteeringSafetyTest):
  STANDSTILL_THRESHOLD = 0
  RELAY_MALFUNCTION_ADDR = MSG_HCA_NEW
  RELAY_MALFUNCTION_BUS = 0

  MAX_RATE_UP = 10
  MAX_RATE_DOWN = 10
  MAX_TORQUE = 370
  MAX_RT_DELTA = 188
  RT_INTERVAL = 250000

  DRIVER_TORQUE_ALLOWANCE = 80
  DRIVER_TORQUE_FACTOR = 3

  cruise_enabled = False

  @classmethod
  def setUpClass(cls):
    if cls.__name__ == "TestVolkswagenMqbEvoSafety":
      cls.packer = None
      cls.safety = None
      raise unittest.SkipTest

  def _speed_msg(self, speed):
    values = {"WHEEL_SPEED_%s" % s: speed for s in ["FL", "FR", "RL", "RR"]}
    return self.packer.make_can_msg_panda("ESP_NEW_1", 0, values)

  def _esp_new_3_msg(self, brake):
    values = {"BRAKE_PRESSED_1": brake}
    return self.packer.make_can_msg_panda("ESP_NEW_3", 0, values)

  def _motor_14_msg(self, brake):
    values = {"MO_Fahrer_bremst": brake}
    return self.packer.make_can_msg_panda("Motor_14", 0, values)

  def _user_brake_msg(self, brake):
    return self._motor_14_msg(brake)

  def _motor_new_1_msg(self, accel_pedal=0):
    tsk_status = 3 if self.cruise_enabled else 2
    values = {
      "TSK_STATUS": tsk_status,
      "ACCEL_PEDAL": accel_pedal,
    }
    return self.packer.make_can_msg_panda("MOTOR_NEW_1", 0, values)

  def _pcm_status_msg(self, enable):
    self.cruise_enabled = enable
    return self._motor_new_1_msg()

  def _user_gas_msg(self, gas):
    # common tests are written assuming separate messages for cruise state and accel pedal, this works around the issue
    self.cruise_enabled = self.safety.get_controls_allowed()
    return self._motor_new_1_msg(accel_pedal=gas)

  def _torque_driver_msg(self, torque):
    values = {"EPS_Lenkmoment": abs(torque), "EPS_VZ_Lenkmoment": torque < 0}
    return self.packer.make_can_msg_panda("LH_EPS_03", 0, values)

  def _torque_cmd_msg(self, torque, steer_req=1):
    values = {"ASSIST_TORQUE": abs(torque), "ASSIST_DIRECTION": torque < 0}
    return self.packer.make_can_msg_panda("HCA_NEW", 0, values)

  def _gra_acc_01_msg(self, cancel=0, resume=0, _set=0, bus=2):
    values = {"GRA_Abbrechen": cancel, "GRA_Tip_Setzen": _set, "GRA_Tip_Wiederaufnahme": resume}
    return self.packer.make_can_msg_panda("GRA_ACC_01", bus, values)

  def test_redundant_brake_signals(self):
    test_combinations = [(True, True, True), (True, True, False), (True, False, True), (False, False, False)]
    for brake_pressed, motor_14_signal, esp_new_3_signal in test_combinations:
      self._rx(self._motor_14_msg(False))
      self._rx(self._esp_new_3_msg(False))
      self.assertFalse(self.safety.get_brake_pressed_prev())
      self._rx(self._motor_14_msg(motor_14_signal))
      self._rx(self._esp_new_3_msg(esp_new_3_signal))
      self.assertEqual(brake_pressed, self.safety.get_brake_pressed_prev(),
                       f"expected {brake_pressed=} with {motor_14_signal=} and {esp_new_3_signal=}")

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


class TestVolkswagenMqbEvoStockSafety(TestVolkswagenMqbEvoSafety):
  TX_MSGS = [[MSG_HCA_NEW, 0], [MSG_GRA_ACC_01, 0], [MSG_GRA_ACC_01, 2]]
  FWD_BLACKLISTED_ADDRS = {2: [MSG_HCA_NEW]}
  FWD_BUS_LOOKUP = {0: 2, 2: 0}

  def setUp(self):
    self.packer = CANPackerPanda("vw_mqbevo")
    self.safety = libpanda_py.libpanda
    self.safety.set_safety_hooks(Panda.SAFETY_VOLKSWAGEN_MQBEVO, 0)
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
