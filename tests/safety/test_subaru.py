#!/usr/bin/env python3
import unittest
from panda import Panda
from panda.tests.libpanda import libpanda_py
import panda.tests.safety.common as common
from panda.tests.safety.common import CANPackerPanda


class TestSubaruSafety(common.PandaSafetyTest, common.DriverTorqueSteeringSafetyTest):
  TX_MSGS = [[0x122, 0], [0x221, 0], [0x321, 0], [0x322, 0], [0x323, 0]]
  STANDSTILL_THRESHOLD = 0  # kph
  RELAY_MALFUNCTION_ADDR = 0x122
  RELAY_MALFUNCTION_BUS = 0
  FWD_BLACKLISTED_ADDRS = {2: [0x122, 0x321, 0x322, 0x323]}
  FWD_BUS_LOOKUP = {0: 2, 2: 0}

  MAX_RATE_UP = 50
  MAX_RATE_DOWN = 70
  MAX_TORQUE = 2047

  MAX_RT_DELTA = 940
  RT_INTERVAL = 250000

  DRIVER_TORQUE_ALLOWANCE = 60
  DRIVER_TORQUE_FACTOR = 50

  ALT_BUS = 0

  def setUp(self):
    self.packer = CANPackerPanda("subaru_global_2017_generated")
    self.safety = libpanda_py.libpanda
    self.safety.set_safety_hooks(Panda.SAFETY_SUBARU, 0)
    self.safety.init_tests()

  def _set_prev_torque(self, t):
    self.safety.set_desired_torque_last(t)
    self.safety.set_rt_torque_last(t)

  # TODO: this is unused
  def _torque_driver_msg(self, torque):
    values = {"Steer_Torque_Sensor": torque}
    return self.packer.make_can_msg_panda("Steering_Torque", 0, values)

  def _speed_msg(self, speed):
    # subaru safety doesn't use the scaled value, so undo the scaling
    values = {s: speed * 0.057 for s in ["FR", "FL", "RR", "RL"]}
    return self.packer.make_can_msg_panda("Wheel_Speeds", self.ALT_BUS, values)

  def _user_brake_msg(self, brake):
    values = {"Brake": brake}
    return self.packer.make_can_msg_panda("Brake_Status", self.ALT_BUS, values)

  def _torque_cmd_msg(self, torque, steer_req=1):
    values = {"LKAS_Output": torque}
    return self.packer.make_can_msg_panda("ES_LKAS", 0, values)

  def _user_gas_msg(self, gas):
    values = {"Throttle_Pedal": gas}
    return self.packer.make_can_msg_panda("Throttle", 0, values)

  def _pcm_status_msg(self, enable):
    values = {"Cruise_Activated": enable}
    return self.packer.make_can_msg_panda("CruiseControl", self.ALT_BUS, values)


class TestSubaruGen2Safety(TestSubaruSafety):
  TX_MSGS = [[0x122, 0], [0x221, 1], [0x321, 0], [0x322, 0], [0x323, 0]]
  ALT_BUS = 1

  MAX_RATE_UP = 40
  MAX_RATE_DOWN = 40
  MAX_TORQUE = 1000

  def setUp(self):
    self.packer = CANPackerPanda("subaru_global_2017_generated")
    self.safety = libpanda_py.libpanda
    self.safety.set_safety_hooks(Panda.SAFETY_SUBARU, Panda.FLAG_SUBARU_GEN2)
    self.safety.init_tests()

class TestSubaruLongitudinalSafety(TestSubaruSafety):
  TX_MSGS = [[0x122, 0], [0x220, 0], [0x221, 0], [0x222, 0], [0x321, 0], [0x322, 0], [0x323, 0], [0x240, 2], [0x13c, 2]]
  FWD_BLACKLISTED_ADDRS = {0: [0x240, 0x13c], 2: [0x122, 0x220, 0x221, 0x222, 0x321, 0x322, 0x323]}

  MAX_RPM = 3200
  MAX_BRAKE = 400
  MAX_THROTTLE = 3400

  def setUp(self):
    self.packer = CANPackerPanda("subaru_global_2017_generated")
    self.safety = libpanda_py.libpanda
    self.safety.set_safety_hooks(Panda.SAFETY_SUBARU, Panda.FLAG_SUBARU_LONG)
    self.safety.init_tests()

  def _es_brake_msg(self, brake=0):
    values = {"Brake_Pressure": brake}
    return self.packer.make_can_msg_panda("ES_Brake", 0, values)

  def _es_distance_msg(self, throttle=0):
    values = {"Cruise_Throttle": throttle}
    return self.packer.make_can_msg_panda("ES_Distance", 0, values)

  def _es_status_msg(self, rpm=0):
    values = {"Cruise_RPM": rpm}
    return self.packer.make_can_msg_panda("ES_Status", 0, values)

  def test_es_brake_msg(self):
    self.safety.set_controls_allowed(1)
    self.assertTrue(self._tx(self._es_brake_msg()))
    self.assertTrue(self._tx(self._es_brake_msg(brake=self.MAX_BRAKE)))
    self.assertFalse(self._tx(self._es_brake_msg(brake=self.MAX_BRAKE+10)))
    self.safety.set_subaru_aeb(1)
    self.assertTrue(self._tx(self._es_brake_msg(brake=self.MAX_BRAKE+10)))

  def test_es_distance_msg(self):
    self.safety.set_controls_allowed(1)
    self.assertTrue(self._tx(self._es_distance_msg()))
    self.assertTrue(self._tx(self._es_distance_msg(throttle=self.MAX_THROTTLE)))
    self.assertFalse(self._tx(self._es_distance_msg(throttle=self.MAX_THROTTLE+1)))

  def test_es_status_msg(self):
    self.safety.set_controls_allowed(1)
    self.assertTrue(self._tx(self._es_status_msg()))
    self.assertTrue(self._tx(self._es_status_msg(rpm=self.MAX_RPM)))
    self.assertFalse(self._tx(self._es_status_msg(rpm=self.MAX_RPM+1)))


if __name__ == "__main__":
  unittest.main()
