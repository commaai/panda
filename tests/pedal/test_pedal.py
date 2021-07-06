import time
import unittest
from panda import Panda
from panda_jungle import PandaJungle  # pylint: disable=import-error
import sys
sys.path.append(os.path.join(os.path.dirname(os.path.realpath(__file__)), "."))
from canhandle import CanHandle

class TestPedal(unittest.TestCase):
  PEDAL_BUS = 1
  def setUp(self):
    self.jungle = PandaJungle()
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
      PandaJungle.flash_static(CanHandle(self.jungle, bus), code.read())

  def test_1_flash_over_can(self):
    self._flash_over_can(self.PEDAL_BUS, "/tmp/pedal.bin.signed")
    time.sleep(10)
    pandas_list = Panda.list()

    self._flash_over_can(self.PEDAL_BUS, "/tmp/pedal_usb.bin.signed")
    time.sleep(10)
    pedal_uid = (set(Panda.list()) ^ set(pandas_list)).pop()

    p = Panda(pedal_uid)
    self.assertTrue(p.is_pedal())
    p.close()

  def test_2_can_spam(self):
    self.jungle.can_clear(0xFFFF)
    rounds = 10
    msgs = 0
    while rounds > 0:
      incoming = self.jungle.can_recv()
      for message in incoming:
        address, _, _, bus = message
        if address == 0x201 and bus == self.PEDAL_BUS:
          msgs += 1
      time.sleep(0.1)
      rounds -= 1
    
    self.assertTrue(msgs > 40)
    print(f"Got {msgs} messages")

  def test_3_silent_mode(self):
    p = Panda('none')
    p.set_pedal_silent(True)
    p.close()

    self.jungle.can_clear(0xFFFF)
    rounds = 10
    msgs = 0
    while rounds > 0:
      incoming = self.jungle.can_recv()
      for message in incoming:
        address, _, _, bus = message
        if address == 0x201 and bus == self.PEDAL_BUS:
          msgs += 1
      time.sleep(0.1)
      rounds -= 1

    self.assertTrue(msgs == 0)
    print(f"Got {msgs} messages")


if __name__ == '__main__':
  unittest.main()
