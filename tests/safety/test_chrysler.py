#!/usr/bin/env python3
import unittest
import itertools

from panda import Panda
from panda.tests.libpanda import libpanda_py
import panda.tests.safety.common as common
from panda.tests.safety.common import CANPackerPanda


class TestChryslerSafety(common.PandaCarSafetyTest, common.MotorTorqueSteeringSafetyTest):
  TX_MSGS = [[0x23B, 0], [0x292, 0], [0x2A6, 0]]
  STANDSTILL_THRESHOLD = 0
  RELAY_MALFUNCTION_ADDRS = {0: (0x292,)}
  FWD_BLACKLISTED_ADDRS = {2: [0x292, 0x2A6]}
  FWD_BUS_LOOKUP = {0: 2, 2: 0}

  MAX_RATE_UP = 3
  MAX_RATE_DOWN = 3
  MAX_TORQUE = 261
  MAX_RT_DELTA = 112
  RT_INTERVAL = 250000
  MAX_TORQUE_ERROR = 80

  LKAS_ACTIVE_VALUE = 1

  DAS_BUS = 0

  cnt_button = 0

  def setUp(self):
    self.packer = CANPackerPanda("chrysler_pacifica_2017_hybrid_generated")
    self.safety = libpanda_py.libpanda
    self.safety.set_safety_hooks(Panda.SAFETY_CHRYSLER, 0)
    self.safety.init_tests()

  def _button_msg(self, cancel=False, resume=False, accel=False, decel=False):
    values = {"ACC_Cancel": cancel, "ACC_Resume": resume, "ACC_Accel": accel, "ACC_Decel": decel, "COUNTER": self.cnt_button}
    self.__class__.cnt_button += 1
    return self.packer.make_can_msg_panda("CRUISE_BUTTONS", self.DAS_BUS, values)

  def _pcm_status_msg(self, enable):
    values = {"ACC_ACTIVE": enable}
    return self.packer.make_can_msg_panda("DAS_3", self.DAS_BUS, values)

  def _speed_msg(self, speed):
    values = {"SPEED_LEFT": speed, "SPEED_RIGHT": speed}
    return self.packer.make_can_msg_panda("SPEED_1", 0, values)

  def _user_gas_msg(self, gas):
    values = {"Accelerator_Position": gas}
    return self.packer.make_can_msg_panda("ECM_5", 0, values)

  def _user_brake_msg(self, brake):
    values = {"Brake_Pedal_State": 1 if brake else 0}
    return self.packer.make_can_msg_panda("ESP_1", 0, values)

  def _torque_meas_msg(self, torque):
    values = {"EPS_TORQUE_MOTOR": torque}
    return self.packer.make_can_msg_panda("EPS_2", 0, values)

  def _torque_cmd_msg(self, torque, steer_req=1):
    values = {"STEERING_TORQUE": torque, "LKAS_CONTROL_BIT": self.LKAS_ACTIVE_VALUE if steer_req else 0}
    return self.packer.make_can_msg_panda("LKAS_COMMAND", 0, values)

  def test_buttons(self):
    for controls_allowed in (True, False):
      self.safety.set_controls_allowed(controls_allowed)

      # resume only while controls allowed
      self.assertEqual(controls_allowed, self._tx(self._button_msg(resume=True)))

      # can always cancel
      self.assertTrue(self._tx(self._button_msg(cancel=True)))

      # only one button at a time
      self.assertFalse(self._tx(self._button_msg(cancel=True, resume=True)))
      self.assertFalse(self._tx(self._button_msg(cancel=False, resume=False)))


class TestChryslerRamDTSafety(TestChryslerSafety):
  TX_MSGS = [[0xB1, 2], [0xA6, 0], [0xFA, 0]]
  RELAY_MALFUNCTION_ADDRS = {0: (0xA6,)}
  FWD_BLACKLISTED_ADDRS = {2: [0xA6, 0xFA]}

  MAX_RATE_UP = 6
  MAX_RATE_DOWN = 6
  MAX_TORQUE = 350

  DAS_BUS = 2

  LKAS_ACTIVE_VALUE = 2

  def setUp(self):
    self.packer = CANPackerPanda("chrysler_ram_dt_generated")
    self.safety = libpanda_py.libpanda
    self.safety.set_safety_hooks(Panda.SAFETY_CHRYSLER, Panda.FLAG_CHRYSLER_RAM_DT)
    self.safety.init_tests()

  def _speed_msg(self, speed):
    values = {"Vehicle_Speed": speed}
    return self.packer.make_can_msg_panda("ESP_8", 0, values)


class TestChryslerRamHDSafety(TestChryslerSafety):
  TX_MSGS = [[0x23A, 2], [0x275, 0], [0x276, 0]]
  RELAY_MALFUNCTION_ADDRS = {0: (0x276,)}
  FWD_BLACKLISTED_ADDRS = {2: [0x275, 0x276]}

  MAX_TORQUE = 361
  MAX_RATE_UP = 14
  MAX_RATE_DOWN = 14
  MAX_RT_DELTA = 182

  DAS_BUS = 2

  LKAS_ACTIVE_VALUE = 2

  def setUp(self):
    self.packer = CANPackerPanda("chrysler_ram_hd_generated")
    self.safety = libpanda_py.libpanda
    self.safety.set_safety_hooks(Panda.SAFETY_CHRYSLER, Panda.FLAG_CHRYSLER_RAM_HD)
    self.safety.init_tests()

  def _speed_msg(self, speed):
    values = {"Vehicle_Speed": speed}
    return self.packer.make_can_msg_panda("ESP_8", 0, values)


class ChryslerLongitudinalBase(TestChryslerSafety):

  DAS_BUS = 0

  MIN_ENGINE_TORQUE = -500
  MAX_ENGINE_TORQUE = 500
  INACTIVE_ENGINE_TORQUE = 0
  MIN_POSSIBLE_ENGINE_TORQUE = -500
  MAX_POSSIBLE_ENGINE_TORQUE = 1548

  MIN_ACCEL = -3.5
  MAX_ACCEL = 2.0
  INACTIVE_ACCEL = 4.0
  MIN_POSSIBLE_ACCEL = -16.0
  MAX_POSSIBLE_ACCEL = 4.0

  # override these tests from PandaCarSafetyTest, chrysler longitudinal uses button enable
  def test_disable_control_allowed_from_cruise(self):
    pass

  def test_enable_control_allowed_from_cruise(self):
    pass

  def test_cruise_engaged_prev(self):
    pass

  def _pcm_status_msg(self, enable):
    # TODO: falsify the above tests
    raise Exception

  def _send_torque_accel_msg(self, enable: bool, torque_active: bool, torque: float, accel_active: int, accel: float):
    values = {
      "ACC_AVAILABLE": 1,
      "ACC_ACTIVE": int(enable),
      "ENGINE_TORQUE_REQUEST": torque,
      "ENGINE_TORQUE_REQUEST_MAX": int(torque_active),
      "ACC_DECEL": accel,
      "ACC_DECEL_REQ": accel_active,
    }
    return self.packer.make_can_msg_panda("DAS_3", self.DAS_BUS, values)

  def _send_accel_msg(self, accel):
    return self._send_torque_accel_msg(True, False, self.INACTIVE_ENGINE_TORQUE, 1, accel)

  def _send_torque_msg(self, torque):
    return self._send_torque_accel_msg(True, True, torque, 0, self.INACTIVE_ACCEL)

  def test_accel_torque_safety_check(self):
    self._generic_limit_safety_check(self._send_accel_msg,
                                     self.MIN_ACCEL, self.MAX_ACCEL,
                                     self.MIN_POSSIBLE_ACCEL, self.MAX_POSSIBLE_ACCEL,
                                     test_delta=0.1, inactive_value=self.INACTIVE_ACCEL)
    self._generic_limit_safety_check(self._send_torque_msg,
                                     self.MIN_ENGINE_TORQUE, self.MAX_ENGINE_TORQUE,
                                     self.MIN_POSSIBLE_ENGINE_TORQUE, self.MAX_POSSIBLE_ENGINE_TORQUE,
                                     test_delta=10, inactive_value=self.INACTIVE_ENGINE_TORQUE)

  def test_buttons(self):
    enable_buttons = {1 << 2: "resume", 1 << 3: "accel", 1 << 4: "decel"}
    for cancel_cur, resume_cur, accel_cur, decel_cur in itertools.product([0, 1], repeat=4):
      for cancel_prev, resume_prev, accel_prev, decel_prev in itertools.product([0, 1], repeat=4):
        self._rx(self._button_msg(cancel=False, resume=False, accel=False, decel=False))
        self.safety.set_controls_allowed(False)
        for _ in range(10):
          self._rx(self._button_msg(cancel_prev, resume_prev, accel_prev, decel_prev))
          self.assertFalse(self.safety.get_controls_allowed())

        # should enter controls allowed on falling edge and not transitioning to cancel
        button_cur = enable_buttons.get(cancel_cur if cancel_cur else (resume_cur << 2) | (accel_cur << 3) | (decel_cur << 4))
        button_prev = enable_buttons.get(cancel_prev if cancel_prev else (resume_prev << 2) | (accel_prev << 3) | (decel_prev << 4))
        should_enable = not cancel_cur and not cancel_prev and button_prev is not None and button_prev != button_cur

        self._rx(self._button_msg(cancel_cur, resume_cur, accel_cur, decel_cur))
        self.assertEqual(should_enable, self.safety.get_controls_allowed())


class TestChryslerLongitudinalSafety(ChryslerLongitudinalBase, TestChryslerSafety):
  TX_MSGS = [[0x23B, 0], [0x292, 0], [0x2A6, 0], [0x1F4, 0], [0x1F5, 0], [0x271, 0]]
  FWD_BLACKLISTED_ADDRS = {2: [0x292, 0x2A6, 0x1F4, 0x1F5, 0x271]}

  def setUp(self):
    self.packer = CANPackerPanda("chrysler_pacifica_2017_hybrid_generated")
    self.safety = libpanda_py.libpanda
    self.safety.set_safety_hooks(Panda.SAFETY_CHRYSLER, Panda.FLAG_CHRYSLER_LONG)
    self.safety.init_tests()


class TestChryslerRamDTLongitudinalSafety(ChryslerLongitudinalBase, TestChryslerRamDTSafety):
  TX_MSGS = [[0xB1, 0], [0xA6, 0], [0xFA, 0], [0x99, 0], [0xE8, 0], [0xA3, 0]]
  FWD_BLACKLISTED_ADDRS = {2: [0xA6, 0xFA, 0x99, 0xE8, 0xA3]}

  def setUp(self):
    self.packer = CANPackerPanda("chrysler_ram_dt_generated")
    self.safety = libpanda_py.libpanda
    self.safety.set_safety_hooks(Panda.SAFETY_CHRYSLER, Panda.FLAG_CHRYSLER_RAM_DT | Panda.FLAG_CHRYSLER_LONG)
    self.safety.init_tests()


class TestChryslerRamHDLongitudinalSafety(ChryslerLongitudinalBase, TestChryslerRamHDSafety):
  TX_MSGS = [[0x23A, 0], [0x275, 0], [0x276, 0], [0x1F4, 0], [0x1F5, 0], [0x271, 0]]
  FWD_BLACKLISTED_ADDRS = {2: [0x275, 0x276, 0x1F4, 0x1F5, 0x271]}

  def setUp(self):
    self.packer = CANPackerPanda("chrysler_ram_hd_generated")
    self.safety = libpanda_py.libpanda
    self.safety.set_safety_hooks(Panda.SAFETY_CHRYSLER, Panda.FLAG_CHRYSLER_RAM_HD | Panda.FLAG_CHRYSLER_LONG)
    self.safety.init_tests()


if __name__ == "__main__":
  unittest.main()
