#!/usr/bin/env python3
import unittest
from panda import Panda
from panda.tests.safety import libpandasafety_py
import panda.tests.safety.common as common
from panda.tests.safety.common import CANPackerPanda
from panda.tests.safety.test_hyundai import HyundaiButtonBase

class TestHyundaiCanfdBase(HyundaiButtonBase, common.PandaSafetyTest, common.DriverTorqueSteeringSafetyTest):

  TX_MSGS = [[0x50, 0], [0x1CF, 1], [0x2A4, 0]]
  STANDSTILL_THRESHOLD = 30  # ~1kph
  RELAY_MALFUNCTION_ADDR = 0x50
  RELAY_MALFUNCTION_BUS = 0
  FWD_BLACKLISTED_ADDRS = {2: [0x50, 0x2a4]}
  FWD_BUS_LOOKUP = {0: 2, 2: 0}

  MAX_RATE_UP = 2
  MAX_RATE_DOWN = 3
  MAX_TORQUE = 270

  MAX_RT_DELTA = 112
  RT_INTERVAL = 250000

  DRIVER_TORQUE_ALLOWANCE = 250
  DRIVER_TORQUE_FACTOR = 2

  PT_BUS = 0
  STEER_MSG = ""

  @classmethod
  def setUpClass(cls):
    if cls.__name__ == "TestHyundaiCanfdBase":
      cls.packer = None
      cls.safety = None
      raise unittest.SkipTest

  def _torque_driver_msg(self, torque):
    values = {"STEERING_COL_TORQUE": torque}
    return self.packer.make_can_msg_panda("MDPS", self.PT_BUS, values)

  def _torque_cmd_msg(self, torque, steer_req=1):
    values = {"TORQUE_REQUEST": torque}
    return self.packer.make_can_msg_panda(self.STEER_MSG, 0, values)

  def _speed_msg(self, speed):
    values = {f"WHEEL_SPEED_{i}": speed * 0.03125 for i in range(1, 5)}
    return self.packer.make_can_msg_panda("WHEEL_SPEEDS", self.PT_BUS, values)

  def _user_brake_msg(self, brake):
    values = {"BRAKE_PRESSED": brake}
    return self.packer.make_can_msg_panda("BRAKE", self.PT_BUS, values)

  def _user_gas_msg(self, gas):
    values = {"ACCELERATOR_PEDAL": gas}
    return self.packer.make_can_msg_panda("ACCELERATOR", self.PT_BUS, values)

  def _pcm_status_msg(self, enable):
    values = {"CRUISE_ACTIVE": enable}
    return self.packer.make_can_msg_panda("SCC1", self.PT_BUS, values)

  def _button_msg(self, buttons, main_button=0, bus=1):
    values = {
      "CRUISE_BUTTONS": buttons,
      "ADAPTIVE_CRUISE_MAIN_BTN": main_button,
    }
    return self.packer.make_can_msg_panda("CRUISE_BUTTONS", self.PT_BUS, values)


class TestHyundaiCanfdHDA1(TestHyundaiCanfdBase):

  TX_MSGS = [[0x12A, 0], [0x1A0, 1], [0x1CF, 0], [0x1E0, 0]]
  RELAY_MALFUNCTION_ADDR = 0x12A
  RELAY_MALFUNCTION_BUS = 0
  FWD_BLACKLISTED_ADDRS = {2: [0x12A, 0x1E0]}
  FWD_BUS_LOOKUP = {0: 2, 2: 0}

  STEER_MSG = "LFA"

  def setUp(self):
    self.packer = CANPackerPanda("hyundai_canfd")
    self.safety = libpandasafety_py.libpandasafety
    self.safety.set_safety_hooks(Panda.SAFETY_HYUNDAI_CANFD, 0)
    self.safety.init_tests()

  def _user_gas_msg(self, gas):
    values = {"ACCELERATOR_PEDAL": gas}
    return self.packer.make_can_msg_panda("ACCELERATOR_ALT", self.PT_BUS, values)

class TestHyundaiCanfdHDA1AltButtons(TestHyundaiCanfdHDA1):

  def setUp(self):
    self.packer = CANPackerPanda("hyundai_canfd")
    self.safety = libpandasafety_py.libpandasafety
    self.safety.set_safety_hooks(Panda.SAFETY_HYUNDAI_CANFD, Panda.FLAG_HYUNDAI_CANFD_ALT_BUTTONS)
    self.safety.init_tests()

  def _button_msg(self, buttons, main_button=0, bus=1):
    values = {
      "CRUISE_BUTTONS": buttons,
      "ADAPTIVE_CRUISE_MAIN_BTN": main_button,
    }
    return self.packer.make_can_msg_panda("CRUISE_BUTTONS_ALT", self.PT_BUS, values)

  def test_button_sends(self):
    """
      No button send allowed with alt buttons.
    """
    for enabled in (True, False):
      for btn in range(8):
        self.safety.set_controls_allowed(enabled)
        self.assertFalse(self._tx(self._button_msg(btn)))


class TestHyundaiCanfdHDA2(TestHyundaiCanfdBase):

  TX_MSGS = [[0x50, 0], [0x1CF, 1], [0x2A4, 0]]
  RELAY_MALFUNCTION_ADDR = 0x50
  RELAY_MALFUNCTION_BUS = 0
  FWD_BLACKLISTED_ADDRS = {2: [0x50, 0x2a4]}
  FWD_BUS_LOOKUP = {0: 2, 2: 0}

  PT_BUS = 1
  STEER_MSG = "LKAS"

  def setUp(self):
    self.packer = CANPackerPanda("hyundai_canfd")
    self.safety = libpandasafety_py.libpandasafety
    self.safety.set_safety_hooks(Panda.SAFETY_HYUNDAI_CANFD, Panda.FLAG_HYUNDAI_CANFD_HDA2)
    self.safety.init_tests()




if __name__ == "__main__":
  unittest.main()
