#!/usr/bin/env python3
import unittest
from panda import Panda
from panda.tests.safety import libpandasafety_py
import panda.tests.safety.common as common
from panda.tests.safety.common import CANPackerPanda

MAX_RATE_UP = 10
MAX_RATE_DOWN = 25
MAX_STEER = 2047

MAX_RT_DELTA = 940
RT_INTERVAL = 250000

DRIVER_TORQUE_ALLOWANCE = 15
DRIVER_TORQUE_FACTOR = 1


class TestMazdaSafety(common.PandaSafetyTest):
  cnt_gas = 0
  cnt_torque_driver = 0
  cnt_cruise = 0
  cnt_speed = 0
  cnt_brake = 0

  TX_MSGS = [[0x243, 0]]
  STANDSTILL_THRESHOLD = 1  # 1kph (see dbc file)
  RELAY_MALFUNCTION_ADDR = 0x243
  RELAY_MALFUNCTION_BUS = 0
  FWD_BLACKLISTED_ADDRS = {2: [0x243]}
  FWD_BUS_LOOKUP = {0: 2, 2: 0}

  def setUp(self):
    self.packer = CANPackerPanda("mazda_cx5_gt_2017")
    self.safety = libpandasafety_py.libpandasafety
    self.safety.set_safety_hooks(Panda.SAFETY_MAZDA, 0)
    self.safety.init_tests()

  def _torque_meas_msg(self, torque):
    values = {"STEER_TORQUE_MOTOR": torque}
    return self.packer.make_can_msg_panda("STEER_TORQUE", 0, values)

#  def _torque_driver_msg(self, torque):
#    values = {"STEER_TORQUE_DRIVER": torque}
#    return self.packer.make_can_msg_panda("STEER_TORQUE", 0, values)

  def _torque_msg(self, torque):
    values = {"LKAS_REQUEST": torque}
    return self.packer.make_can_msg_panda("CAM_LKAS", 0, values)

  def _accel_msg(self, accel):
    values = {"SET_P": accel}
    return self.packer.make_can_msg_panda("CRZ_BTNS", 0, values)

  def _speed_msg(self, s):
    values = {("%s"%n): s for n in ["FR", "FL", "RR", "RL"]}
    return self.packer.make_can_msg_panda("WHEEL_SPEEDS", 0, values)

  def _brake_msg(self, pressed):
    values = {"BRAKE_ON": pressed}
    return self.packer.make_can_msg_panda("PEDALS", 0, values)

  def _gas_msg(self, pressed):
    values = {"PEDAL_GAS": pressed}
    return self.packer.make_can_msg_panda("ENGINE_DATA", 0, values)

  def _pcm_status_msg(self, cruise_on):
    values = {"CRZ_ACTIVE": cruise_on}
    return self.packer.make_can_msg_panda("CRZ_CTRL", 0, values)

  def test_rx_hook(self):
    # checksum checks
    for msg in ["trq", "pcm"]:
      self.safety.set_controls_allowed(1)
      if msg == "trq":
        to_push = self._torque_meas_msg(0)
      if msg == "pcm":
        to_push = self._pcm_status_msg(True)
      self.assertTrue(self._rx(to_push))
      to_push[0].RDHR = 0
      self.assertFalse(self._rx(to_push))
      self.assertFalse(self.safety.get_controls_allowed())


if __name__ == "__main__":
  unittest.main()
