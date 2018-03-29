#!/usr/bin/env python2
import unittest
import numpy as np
import libpandasafety_py


def twos_comp(val, bits):
  if val >= 0:
    return val
  else:
    return (2**bits) + val


class TestToyotaSafety(unittest.TestCase):
  @classmethod
  def setUpClass(cls):
    cls.safety = libpandasafety_py.libpandasafety
    cls.safety.toyota_init(100)

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

  def test_accel_actuation_limits(self):
    to_send = libpandasafety_py.ffi.new('CAN_FIFOMailBox_TypeDef *')
    to_send[0].RIR = 0x343 << 21

    MAX_ACCEL = 1500
    MIN_ACCEL = -3000

    for accel in np.arange(-4000, 4000, 100):
      for controls_allowed in [True, False]:
        a = twos_comp(accel, 16)
        to_send[0].RDLR = (a & 0xFF) << 8 | (a >> 8)

        self.safety.set_controls_allowed(controls_allowed)

        if controls_allowed:
          send = MIN_ACCEL <= accel <= MAX_ACCEL
        else:
          send = accel == 0
        self.assertEqual(send, self.safety.toyota_tx_hook(to_send))

if __name__ == "__main__":
  unittest.main()
