#!/usr/bin/env python3
import random
import unittest
from unittest import mock

from panda import Panda, pack_can_buffer, unpack_can_buffer, CANPACKET_HEAD_SIZE, DLC_TO_LEN
from panda.python.spi import PandaSpiNackResponse


def make_panda() -> Panda:
  p = Panda.__new__(Panda)
  p._handle = mock.Mock()
  p.can_version = Panda.CAN_PACKET_VERSION
  p.can_rx_overflow_buffer = b''
  return p

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


class TestCanSendNonblocking(unittest.TestCase):
  def test_chunks(self):
    # the same ~256 byte writes as pack_can_buffer(chunk=True), up to the first NACK
    frames = [(i, bytes(DLC_TO_LEN[i % len(DLC_TO_LEN)]), i % 3) for i in range(300)]
    chunks = [bytes(c) for c in pack_can_buffer(frames, chunk=True) if c]
    for nack_at, nwrites in ((None, len(chunks)), (0, 0), (3, 3)):
      p, writes = make_panda(), []
      def write(ep, dat):
        if len(writes) == nack_at:
          raise PandaSpiNackResponse
        writes.append(bytes(dat))
      p._handle.bulkWrite.side_effect = write
      sent = p.can_send_nonblocking(frames)
      assert writes == chunks[:nwrites]
      assert frames[:sent] == [f for w in writes for f in unpack_can_buffer(w)[0]]


class TestCanRecv(unittest.TestCase):
  def test_out_of_sync(self):
    # a bad checksum, or both returned and rejected set (all 0xFF passes the checksum): the buffer's dropped and
    # the panda's CAN comms reset, then it starts over
    good = pack_can_buffer([(1, b'12345678', 0)])[0]
    bad_checksum = bytearray(pack_can_buffer([(2, b'abc', 1)])[0])
    bad_checksum[-1] ^= 0xff
    for bad in (bytes(bad_checksum), b'\xff' * (CANPACKET_HEAD_SIZE + 64)):
      p = make_panda()
      p.can_rx_overflow_buffer = good[:10]  # a partial frame from the last read
      p._handle.bulkRead.side_effect = [good[10:] + bad, b'\x00\x01']
      with mock.patch.object(p, 'can_reset_communications') as reset:
        with self.assertRaises(AssertionError):
          p.can_recv()
        assert p.can_rx_overflow_buffer == b''
        reset.assert_called_once()
        assert p.can_recv() == []
        assert p.can_rx_overflow_buffer == b'\x00\x01'

if __name__ == "__main__":
  unittest.main()
