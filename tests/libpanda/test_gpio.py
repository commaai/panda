#!/usr/bin/env python3
import ctypes
import os
import unittest

from panda import Panda

lib = ctypes.CDLL(os.path.join(os.path.dirname(__file__), "libboard.so"))
bus = ctypes.addressof(ctypes.c_uint8.in_dll(lib, "bus"))
hw_type = ctypes.c_uint8.in_dll(lib, "hw_type")

INPUT, OUTPUT, ALTERNATE, ANALOG = range(4)
NONE, UP, DOWN = range(3)
LOW, HIGH = 0, 1


class GPIO(ctypes.Structure):
  _fields_ = [(r, ctypes.c_uint32) for r in ("MODER", "OTYPER", "OSPEEDR", "PUPDR", "IDR", "ODR", "BSRR", "LCKR")] + [("AFR", ctypes.c_uint32 * 2)]

  def mode(self, pin):
    return self.MODER >> (2 * pin) & 3

  def pull(self, pin):
    return self.PUPDR >> (2 * pin) & 3

  def out(self, pin):
    return self.ODR >> pin & 1

  def open_drain(self, pin):
    return self.OTYPER >> pin & 1

  def af(self, pin):
    return self.AFR[pin // 8] >> (4 * (pin % 8)) & 0xF


GPIOA, GPIOB, GPIOC, GPIOD, GPIOE, GPIOF, GPIOG = (GPIO.from_address(bus + 0x18020000 + 0x400 * i) for i in range(7))

# ID pins that read high
STRAPS = {
  Panda.HW_TYPE_RED_PANDA: (),
  Panda.HW_TYPE_TRES: ((GPIOF, 8),),
  Panda.HW_TYPE_CUATRO: ((GPIOD, 4), (GPIOD, 5)),
}


def boot(hw):
  lib.power_cycle()
  for gpio, pin in STRAPS[hw]:
    gpio.IDR |= 1 << pin
  lib.boot()
  assert hw_type.value == hw[0]


class TestInit(unittest.TestCase):
  def assertInput(self, gpio, pin, pull):
    self.assertEqual(gpio.mode(pin), INPUT)
    self.assertEqual(gpio.pull(pin), pull)

  def assertOutput(self, gpio, pin, level, open_drain=False):
    self.assertEqual(gpio.mode(pin), OUTPUT)
    self.assertEqual(gpio.out(pin), level)
    self.assertEqual(gpio.open_drain(pin), open_drain)

  def assertAlternate(self, gpio, pin, af, open_drain=False):
    self.assertEqual(gpio.mode(pin), ALTERNATE)
    self.assertEqual(gpio.af(pin), af)
    self.assertEqual(gpio.open_drain(pin), open_drain)

  def test_common(self):
    for hw in STRAPS:
      with self.subTest(hw=hw):
        boot(hw)
        for pin in (11, 12):  # USB
          self.assertAlternate(GPIOA, pin, 10)
        for pin in (8, 9, 5, 6):  # FDCAN1, FDCAN2
          self.assertAlternate(GPIOB, pin, 9)
        for pin in (12, 13):  # FDCAN2, flipped
          self.assertEqual(GPIOB.mode(pin), ANALOG)
        self.assertEqual(GPIOF.mode(11), ANALOG)  # VOLT_S

  def test_red(self):
    boot(Panda.HW_TYPE_RED_PANDA)
    for pin in (9, 10):  # FDCAN3
      self.assertAlternate(GPIOG, pin, 2)
    for gpio, pin in ((GPIOG, 11), (GPIOB, 3), (GPIOD, 7), (GPIOB, 4)):  # transceivers enabled
      self.assertOutput(gpio, pin, LOW)
    for pin in (2, 3, 4):  # LEDs off
      self.assertOutput(GPIOE, pin, HIGH, open_drain=True)

  def test_tres(self):
    boot(Panda.HW_TYPE_TRES)
    for pin in (9, 10):  # FDCAN3
      self.assertAlternate(GPIOG, pin, 2)
    for gpio, pin in ((GPIOB, 10), (GPIOB, 11), (GPIOG, 11), (GPIOD, 7)):  # transceivers enabled
      self.assertOutput(gpio, pin, LOW)
    for pin in (2, 3, 4):  # LEDs off
      self.assertOutput(GPIOE, pin, HIGH, open_drain=True)
    self.assertOutput(GPIOA, 0, LOW)  # bootkick
    self.assertOutput(GPIOC, 12, HIGH)  # SOM out of reset
    self.assertInput(GPIOC, 2, DOWN)  # SOM GPIO
    for pin in (8, 9):  # fan, IR on TIM3
      self.assertAlternate(GPIOC, pin, 2)
    for pin in (10, 11):  # siren on I2C5
      self.assertAlternate(GPIOC, pin, 4, open_drain=True)
    for pin in (11, 12, 13, 14):  # SPI4
      self.assertAlternate(GPIOE, pin, 5)

  def test_cuatro(self):
    boot(Panda.HW_TYPE_CUATRO)
    for pin in (12, 13):  # FDCAN3
      self.assertAlternate(GPIOD, pin, 5)
    for gpio, pin in ((GPIOB, 7), (GPIOB, 10), (GPIOD, 8), (GPIOB, 11)):  # transceivers enabled
      self.assertOutput(gpio, pin, LOW)
    for pin in (6, 7, 9):  # LEDs on TIM3
      self.assertAlternate(GPIOC, pin, 2, open_drain=True)
    self.assertOutput(GPIOA, 0, LOW)  # bootkick
    self.assertOutput(GPIOC, 11, LOW, open_drain=True)  # DC_IN_EN_N
    self.assertInput(GPIOC, 2, DOWN)  # SOM GPIO
    self.assertAlternate(GPIOC, 8, 2, open_drain=True)  # fan on TIM3
    self.assertOutput(GPIOB, 0, LOW)  # amp off
    for gpio, pin in ((GPIOC, 5), (GPIOA, 6)):  # power readout
      self.assertEqual(gpio.mode(pin), ANALOG)
    for pin in (11, 12, 13, 14):  # SPI4
      self.assertAlternate(GPIOE, pin, 5)


if __name__ == "__main__":
  unittest.main()
