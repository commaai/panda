#!/usr/bin/env python2
import unittest
import libpandasafety_py


class TestToyotaSafety(unittest.TestCase):
  @classmethod
  def setUpClass(cls):
    cls.safety = libpandasafety_py.libpandasafety
    cls.safety.toyota_init(0)

  def test_default_controls_not_allowed(self):
    self.assertFalse(self.safety.get_controls_allowed())

  def test_manually_enable_controls_allowed(self):
    self.safety.set_controls_allowed(1)
    self.assertTrue(self.safety.get_controls_allowed())

  def test_enable_control_allowed_from_cruise(self):
    to_push = libpandasafety_py.ffi.new('CAN_FIFOMailBox_TypeDef *')
    to_push[0].RIR = 0x1D2 << 21
    to_push[0].RDHR = 0xF00000

    self.safety.toyota_rx_hook(to_push)
    self.assertTrue(self.safety.get_controls_allowed())



if __name__ == "__main__":
  unittest.main()
