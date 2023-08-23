#!/usr/bin/env python3
import os
import time
import unittest

from panda import Panda, PandaJungle, CanHandle, McuType, BASEDIR


JUNGLE_SERIAL = os.getenv("PEDAL_JUNGLE")
PEDAL_BUS = 1

class TestPedal(unittest.TestCase):

  def setUp(self):
    self.jungle = PandaJungle(JUNGLE_SERIAL)
    self.jungle.set_panda_power(True)
    self.jungle.set_ignition(False)

  def tearDown(self):
    self.jungle.close()

  def _flash_over_can(self, bus, fw_file):
    print(f"Flashing {fw_file}")
    while len(self.jungle.can_recv()) != 0:
      continue
    self.jungle.can_send(0x200, b"\xce\xfa\xad\xde\x1e\x0b\xb0\x0a", bus)

    time.sleep(0.1)
    with open(fw_file, "rb") as code:
      PandaJungle.flash_static(CanHandle(self.jungle, bus), code.read(), McuType.F2)

  def _listen_can_frames(self):
    self.jungle.can_clear(0xFFFF)
    msgs = 0
    for _ in range(10):
      incoming = self.jungle.can_recv()
      for message in incoming:
        address, _, _, bus = message
        if address == 0x201 and bus == PEDAL_BUS:
          msgs += 1
      time.sleep(0.1)
    return msgs

  def test_usb_fw(self):
    self._flash_over_can(PEDAL_BUS, f"{BASEDIR}/board/pedal/obj/pedal_usb.bin.signed")
    time.sleep(2)
    with Panda('pedal') as p:
      self.assertTrue(p.get_type() == Panda.HW_TYPE_PEDAL)
    self.assertTrue(self._listen_can_frames() > 40)

  def test_nonusb_fw(self):
    self._flash_over_can(PEDAL_BUS, f"{BASEDIR}board/pedal/obj/pedal.bin.signed")
    time.sleep(2)
    self.assertTrue(self._listen_can_frames() > 40)


if __name__ == '__main__':
  unittest.main()
