import os
import time
import contextlib
import struct
import signal
from nose.tools import assert_greater
import unittest

from panda import Panda
from panda_jungle import PandaJungle  # pylint: disable=import-error

############################# CanHandle #########################
class CanHandle(object):
  def __init__(self, p, bus):
    self.p = p
    self.bus = bus

  def transact(self, dat):
    self.p.isotp_send(1, dat, self.bus, recvaddr=2)

    def _handle_timeout(signum, frame):
      # will happen on reset
      raise Exception("timeout")

    signal.signal(signal.SIGALRM, _handle_timeout)
    signal.alarm(1)
    try:
      ret = self.p.isotp_recv(2, self.bus, sendaddr=1)
    finally:
      signal.alarm(0)

    return ret

  def controlWrite(self, request_type, request, value, index, data, timeout=0):
    # ignore data in reply, panda doesn't use it
    return self.controlRead(request_type, request, value, index, 0, timeout)

  def controlRead(self, request_type, request, value, index, length, timeout=0):
    dat = struct.pack("HHBBHHH", 0, 0, request_type, request, value, index, length)
    return self.transact(dat)

  def bulkWrite(self, endpoint, data, timeout=0):
    if len(data) > 0x10:
      raise ValueError("Data must not be longer than 0x10")
    dat = struct.pack("HH", endpoint, len(data)) + data
    return self.transact(dat)

  def bulkRead(self, endpoint, length, timeout=0):
    dat = struct.pack("HH", endpoint, 0)
    return self.transact(dat)


def flash_over_can(p, bus, fw_file):
  print(f"Flashing {fw_file}")

  while 1:
    if len(p.can_recv()) == 0:
      break

  p.can_send(0x200, b"\xce\xfa\xad\xde\x1e\x0b\xb0\x0a", bus)
  #p.can_send(0x200, b"\xce\xfa\xad\xde\x1e\x0b\xb0\x02", bus) #DFU mode (recover)

  time.sleep(0.1)
  with open(fw_file, "rb") as code:
    PandaJungle.flash_static(CanHandle(p, bus), code.read())

############################# unittest #########################

class TestPedal(unittest.TestCase):
  def setUp(self):
    self.jungle = TestPedal.jungle
    self.pedal_bus = TestPedal.pedal_bus

  @classmethod
  def setUpClass(cls):
    super(TestPedal, cls).setUpClass()
    with open(os.devnull, "w") as devnull:
      with contextlib.redirect_stdout(devnull):
        cls.jungle = PandaJungle()
    cls.jungle.set_panda_power(True)
    cls.jungle.set_ignition(False)
    cls.pedal_bus = -1

  def test_1_pedal_bus(self):
    """
    Find if pedal is connected and to which bus
    """
    self.jungle.can_clear(0xFFFF)
    rounds = 10
    while rounds > 0:
      incoming = self.jungle.can_recv()
      for message in incoming:
        address, unused, data, bus = message
        if address == 0x201:
          self.pedal_bus = bus
          break
      time.sleep(0.1)
      rounds -= 1
    self.assertTrue(self.pedal_bus > -1)
    print(f"Pedal is on bus {self.pedal_bus}")

  def test_2_flash_over_can(self):
    self.pedal_bus = 1
    print(self.pedal_bus)
    flash_over_can(self.jungle, self.pedal_bus, "board/obj/pedal.bin.signed")
    time.sleep(10)
    pandas_list = Panda.list()

    flash_over_can(self.jungle, self.pedal_bus, "board/obj/pedal_usb.bin.signed")
    time.sleep(10)
    pedal_uid = (set(Panda.list()) ^ set(pandas_list)).pop()

    p = Panda(pedal_uid)
    self.assertTrue(p.is_pedal())
    p.close()

  def test_3_can_spam(self):
    self.jungle.can_clear(0xFFFF)
    rounds = 10
    msgs = 0
    while rounds > 0:
      incoming = self.jungle.can_recv()
      for message in incoming:
        address, unused, data, bus = message
        if address == 0x201:
          msgs += 1
      time.sleep(0.1)
      rounds -= 1
    
    self.assertTrue(msgs > 40)
    print(f"Got {msgs} messages")

if __name__ == '__main__':
  unittest.main()
