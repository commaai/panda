#!/usr/bin/env python2
import unittest
import numpy as np
import libpandasafety_py


class TestHondaSafety(unittest.TestCase):
  @classmethod
  def setUp(cls):
    cls.safety = libpandasafety_py.libpandasafety
    cls.safety.honda_init(0)
    cls.safety.init_tests_honda()

  def _speed_msg(self, speed):
    to_send = libpandasafety_py.ffi.new('CAN_FIFOMailBox_TypeDef *')
    to_send[0].RIR = 0x158 << 21
    to_send[0].RDLR = speed

    return to_send

  def _button_msg(self, buttons):
    to_send = libpandasafety_py.ffi.new('CAN_FIFOMailBox_TypeDef *')
    to_send[0].RIR = 0x1A6 << 21
    to_send[0].RDLR = buttons << 5

    return to_send

  def test_default_controls_not_allowed(self):
    self.assertFalse(self.safety.get_controls_allowed())

  def test_resume_button(self):
    RESUME_BTN = 4
    self.safety.honda_rx_hook(self._button_msg(RESUME_BTN))
    self.assertTrue(self.safety.get_controls_allowed())

  def test_set_button(self):
    SET_BTN = 3
    self.safety.honda_rx_hook(self._button_msg(SET_BTN))
    self.assertTrue(self.safety.get_controls_allowed())

  def test_cancel_button(self):
    CANCEL_BTN = 2
    self.safety.set_controls_allowed(1)
    self.safety.honda_rx_hook(self._button_msg(CANCEL_BTN))
    self.assertFalse(self.safety.get_controls_allowed())

  def test_sample_speed(self):
    self.assertEqual(0, self.safety.get_ego_speed())
    self.safety.honda_rx_hook(self._speed_msg(100))
    self.assertEqual(100, self.safety.get_ego_speed())


if __name__ == "__main__":
  unittest.main()
