#!/usr/bin/env python3
import random
import struct
import unittest

from panda import Panda, pack_can_buffer, unpack_can_buffer, DLC_TO_LEN

class PandaTestPackUnpack(unittest.TestCase):
  def test_panda_lib_pack_unpack(self):
    overflow_buf = b''

    to_pack = []
    for _ in range(10000):
      address = random.randint(1, (1 << 29) - 1)
      data = bytes([random.getrandbits(8) for _ in range(DLC_TO_LEN[random.randrange(0, len(DLC_TO_LEN))])])
      to_pack.append((address, data, 0))

    packed = pack_can_buffer(to_pack)
    unpacked = []
    for dat in packed:
      msgs, overflow_buf = unpack_can_buffer(overflow_buf + dat)
      unpacked.extend(msgs)

    self.assertEqual(unpacked, to_pack)

  def test_health_structs_have_no_padding(self):
    # the firmware sends health_t and can_health_t as laid out in memory, python unpacks them without padding
    for s in (Panda.HEALTH_STRUCT, Panda.CAN_HEALTH_STRUCT):
      self.assertEqual(struct.calcsize("@" + s.format[1:] + "0I"), s.size)

  def test_health_unpacking(self):
    class ZerosHandle:
      def controlRead(self, request_type, request, value, index, length):
        return bytes(length)

    p = Panda.__new__(Panda)
    p._handle = ZerosHandle()
    p.health_version = Panda.HEALTH_PACKET_VERSION
    self.assertEqual(p.health()["temperature"], -40.0)
    self.assertEqual(p.can_health(0)["last_error"], "No error")

if __name__ == "__main__":
  unittest.main()
