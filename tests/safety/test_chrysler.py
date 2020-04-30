#!/usr/bin/env python3
import unittest
from panda import Panda
from panda.tests.safety import libpandasafety_py
import panda.tests.safety.common as common
from panda.tests.safety.common import make_msg


def chrysler_checksum(msg, len_msg):
  checksum = 0xFF
  for idx in range(0, len_msg-1):
    curr = (msg.RDLR >> (8*idx)) if idx < 4 else (msg.RDHR >> (8*(idx - 4)))
    curr &= 0xFF
    shift = 0x80
    for i in range(0, 8):
      bit_sum = curr & shift
      temp_chk = checksum & 0x80
      if (bit_sum != 0):
        bit_sum = 0x1C
        if (temp_chk != 0):
          bit_sum = 1
        checksum = checksum << 1
        temp_chk = checksum | 1
        bit_sum ^= temp_chk
      else:
        if (temp_chk != 0):
          bit_sum = 0x1D
        checksum = checksum << 1
        bit_sum ^= checksum
      checksum = bit_sum
      shift = shift >> 1
  return ~checksum & 0xFF

class TestChryslerSafety(common.PandaSafetyTest, common.TorqueSteeringSafetyTest):
  TX_MSGS = [[571, 0], [658, 0], [678, 0]]
  STANDSTILL_THRESHOLD = 0
  RELAY_MALFUNCTION_ADDR = 0x292
  RELAY_MALFUNCTION_BUS = 0
  FWD_BLACKLISTED_ADDRS = {2: [658, 678]}
  FWD_BUS_LOOKUP = {0: 2, 2: 0}

  MAX_RATE_UP = 3
  MAX_RATE_DOWN = 3
  MAX_TORQUE = 261
  MAX_RT_DELTA = 112
  RT_INTERVAL = 250000
  MAX_TORQUE_ERROR = 80

  cnt_torque_meas = 0
  cnt_gas = 0
  cnt_cruise = 0
  cnt_brake = 0

  def setUp(self):
    self.safety = libpandasafety_py.libpandasafety
    self.safety.set_safety_hooks(Panda.SAFETY_CHRYSLER, 0)
    self.safety.init_tests_chrysler()

  def _button_msg(self, buttons):
    to_send = make_msg(0, 571)
    to_send[0].RDLR = buttons
    return to_send

  def _pcm_status_msg(self, active):
    to_send = make_msg(0, 500)
    to_send[0].RDLR = 0x380000 if active else 0
    to_send[0].RDHR |= (self.cnt_cruise % 16) << 20
    to_send[0].RDHR |= chrysler_checksum(to_send[0], 8) << 24
    self.__class__.cnt_cruise += 1
    return to_send

  def _speed_msg(self, speed):
    speed = int(speed / 0.071028)
    to_send = make_msg(0, 514, 4)
    to_send[0].RDLR = ((speed & 0xFF0) >> 4) + ((speed & 0xF) << 12) + \
                      ((speed & 0xFF0) << 12) + ((speed & 0xF) << 28)
    return to_send

  def _gas_msg(self, gas):
    to_send = make_msg(0, 308)
    to_send[0].RDHR = (gas & 0x7F) << 8
    to_send[0].RDHR |= (self.cnt_gas % 16) << 20
    self.__class__.cnt_gas += 1
    return to_send

  def _brake_msg(self, brake):
    to_send = make_msg(0, 320)
    to_send[0].RDLR = 5 if brake else 0
    to_send[0].RDHR |= (self.cnt_brake % 16) << 20
    to_send[0].RDHR |= chrysler_checksum(to_send[0], 8) << 24
    self.__class__.cnt_brake += 1
    return to_send

  def _torque_meas_msg(self, torque):
    to_send = make_msg(0, 544)
    to_send[0].RDHR = ((torque + 1024) >> 8) + (((torque + 1024) & 0xff) << 8)
    to_send[0].RDHR |= (self.cnt_torque_meas % 16) << 20
    to_send[0].RDHR |= chrysler_checksum(to_send[0], 8) << 24
    self.__class__.cnt_torque_meas += 1
    return to_send

  def _torque_msg(self, torque):
    to_send = make_msg(0, 0x292)
    to_send[0].RDLR = ((torque + 1024) >> 8) + (((torque + 1024) & 0xff) << 8)
    return to_send

  def test_disengage_on_gas(self):
    self.safety.set_controls_allowed(1)
    self._rx(self._speed_msg(2.2))
    self._rx(self._gas_msg(1))
    self.assertTrue(self.safety.get_controls_allowed())
    self._rx(self._gas_msg(0))
    self._rx(self._speed_msg(2.3))
    self._rx(self._gas_msg(1))
    self.assertFalse(self.safety.get_controls_allowed())

  def test_cancel_button(self):
    CANCEL = 1
    for b in range(0, 0x1ff):
      if b == CANCEL:
        self.assertTrue(self._tx(self._button_msg(b)))
      else:
        self.assertFalse(self._tx(self._button_msg(b)))


if __name__ == "__main__":
  unittest.main()
