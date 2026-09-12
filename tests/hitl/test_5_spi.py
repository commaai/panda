import binascii
import random
import zlib
from unittest.mock import patch

from panda.tests.hitl.base import PandaTestCase
from panda import Panda
from panda.python.spi import PandaProtocolMismatch, PandaSpiHandle, PandaSpiNackResponse, PandaSpiTransferFailed


class TestSpi(PandaTestCase):
  def _ping(self, panda):
    # should work with no retries
    with patch.object(panda._handle.dev, 'xfer2', wraps=panda._handle.dev.xfer2) as spy:
      panda.health()
      assert spy.call_count == 2

  def test_protocol_version_check(self):
    p = self.p
    accept_version = PandaSpiHandle._accept_protocol_version

    def unsupported_version(handle, version):
      # Simulate a firmware descriptor with an unsupported protocol, preserving
      # its serial/type/bootstub fields and the real discovery transaction.
      return accept_version(handle, version[:14] + b"\xff" + version[15:])

    for bootstub in (False, True):
      p.reset(enter_bootstub=bootstub)
      serial = p._serial
      p.close()  # Enumeration must acquire the connection-lifetime SPI lock.
      try:
        with patch.object(PandaSpiHandle, '_accept_protocol_version', unsupported_version):
          assert serial in Panda.list()
          with self.assertRaises(PandaProtocolMismatch):
            Panda(serial)
      finally:
        p.reconnect()

  def test_protocol_version_data(self):
    p = self.p
    for bootstub in (False, True):
      p.reset(enter_bootstub=bootstub)
      v = p._handle.get_protocol_version()

      uid = binascii.hexlify(v[:12]).decode()
      assert uid == p.get_uid()

      hwtype = v[12]
      assert hwtype == ord(p.get_type())

      bstub = v[13]
      assert bstub == (0xEE if bootstub else 0xCC)

  def test_all_comm_types(self):
    p = self.p
    spy = self.enterContext(patch.object(p._handle.dev, 'xfer2', wraps=p._handle.dev.xfer2))

    # controlRead + controlWrite
    p.health()
    p.can_clear(0)
    assert spy.call_count == 2*2

    # bulkRead + bulkWrite
    p.can_recv()
    p.can_send(0x123, b"somedata", 0)
    assert spy.call_count == 2*4

  def test_bad_header(self):
    p = self.p
    try:
      with patch('panda.python.spi.SYNC', 0):
        with self.assertRaises(PandaSpiTransferFailed):
          p._handle.controlRead(Panda.REQUEST_IN, 0xd2, 0, 0, p.HEALTH_STRUCT.size, timeout=50)
      assert not p._handle._healthy
    finally:
      # A timed-out v3 operation has an unknown outcome; recovery needs a new
      # session, rather than retrying application bytes on the invalid handle.
      p.reconnect()
    self._ping(p)

  def test_bad_checksum(self):
    p = self.p
    cnt = p.health()['spi_error_count']
    sequence = p._handle._seq
    transfer = p._handle.dev.xfer2
    corrupted = False

    def corrupt_request(data):
      nonlocal corrupted
      frame = bytes(data)
      if not corrupted and len(frame) >= PandaSpiHandle.FRAME_OVERHEAD and frame[0] == 0x5a:
        corrupted = True
        frame = frame[:-1] + bytes([frame[-1] ^ 1])
        assert zlib.crc32(frame[:-4]) != int.from_bytes(frame[-4:], 'little')
      return transfer(frame)

    with patch.object(p._handle.dev, 'xfer2', side_effect=corrupt_request) as spy:
      p.health()
      assert corrupted
      assert spy.call_count == 4  # rejected request/read, then the same complete pair
    assert p._handle._seq == sequence + 1
    self._ping(p)
    assert (p.health()['spi_error_count'] - cnt) > 0

  def test_non_existent_endpoint(self):
    p = self.p
    for _ in range(10):
      ep = random.randint(4, 20)
      with self.assertRaises(PandaSpiNackResponse):
        p._handle.bulkRead(ep, random.randint(1, 1000), timeout=50)

      self._ping(p)

      with self.assertRaises(PandaSpiNackResponse):
        p._handle.bulkWrite(ep, b"abc", timeout=50)

      self._ping(p)
