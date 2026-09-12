import binascii
import random
import struct
import time
from unittest.mock import patch

from panda.tests.hitl.base import PandaTestCase
from panda import Panda
from panda.python.spi import ACK, FRAME_SIZE, MAX_PAYLOAD, NACK, SYNC, PandaProtocolMismatch, PandaSpiNackResponse, crc8


class TestSpi(PandaTestCase):
  @classmethod
  def setUpClass(cls):
    spi_only = patch.object(Panda, 'usb_connect', return_value=(None, None, None, False))
    spi_only.start()
    cls.addClassCleanup(spi_only.stop)
    super().setUpClass()

  def _ping(self, panda):
    # should work with no retries
    with patch.object(panda._handle, '_transfer_spidev', wraps=panda._handle._transfer_spidev) as spy:
      panda.health()
      assert spy.call_count == 1

  def test_protocol_version_check(self):
    p = self.p
    for bootstub in (False, True):
      p.reset(enter_bootstub=bootstub)
      with patch('panda.python.spi.PandaSpiHandle.PROTOCOL_VERSION', 0):
        # list should still work with wrong version
        assert p._serial in Panda.list()

        # connect but raise protocol error
        with self.assertRaises(PandaProtocolMismatch):
          Panda(p._serial)

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
      assert v[14] == p._handle.PROTOCOL_VERSION

  def test_all_comm_types(self):
    p = self.p
    spy = self.enterContext(patch.object(p._handle, '_transfer_spidev', wraps=p._handle._transfer_spidev))

    # controlRead + controlWrite
    p.health()
    p.can_clear(0)
    assert spy.call_count == 2

    # bulkRead + bulkWrite
    p.can_recv()
    p.can_send(0x123, b"somedata", 0)
    assert spy.call_count == 4

  def test_bad_header(self):
    p = self.p
    count = p.health()['spi_error_count']
    with patch('panda.python.spi.SYNC', 0):
      with self.assertRaises(PandaSpiNackResponse):
        p._handle.controlRead(Panda.REQUEST_IN, 0xd2, 0, 0, p.HEALTH_STRUCT.size, timeout=50)
    self._ping(p)
    assert p.health()['spi_error_count'] > count

  def test_bad_checksum(self):
    p = self.p
    cnt = p.health()['spi_error_count']
    with patch('panda.python.spi.crc8', side_effect=lambda data: 0 if data[0] == SYNC else crc8(data)):
      with self.assertRaises(PandaSpiNackResponse):
        p._handle.controlRead(Panda.REQUEST_IN, 0xd2, 0, 0, p.HEALTH_STRUCT.size, timeout=50)
    self._ping(p)
    assert (p.health()['spi_error_count'] - cnt) > 0

  def _request(self, tx_len=7, rx_len=64):
    packet = bytearray(FRAME_SIZE)
    struct.pack_into('<BBHH', packet, 0, SYNC, 0, tx_len, rx_len)
    struct.pack_into('<BHHH', packet, 6, 0xc1, 0, 0, 1)
    packet[-1] = crc8(packet[:-1])
    return packet

  def test_invalid_lengths(self):
    h = self.p._handle
    for tx_len, rx_len in ((MAX_PAYLOAD + 1, 64), (65535, 64), (7, MAX_PAYLOAD + 1), (7, 65535)):
      with h.dev.acquire() as spi:
        spi.xfer2(self._request(tx_len, rx_len))
        deadline = time.monotonic() + 0.1
        while True:
          reply = spi.xfer2(bytes(FRAME_SIZE))
          if reply[0] == NACK:
            break
          assert time.monotonic() < deadline
        assert crc8(reply[:-1]) == reply[-1]
      self._ping(self.p)

  def test_response_capacity_and_padding(self):
    for bootstub in (False, True):
      self.p.reset(enter_bootstub=bootstub)
      for capacity in (0, 1, 64):
        with self.p._handle.dev.acquire() as spi:
          spi.xfer2(self._request(rx_len=capacity))
          deadline = time.monotonic() + 0.1
          while True:
            reply = spi.xfer2(bytes(FRAME_SIZE))
            if reply[0] == ACK:
              break
            assert time.monotonic() < deadline
          length = struct.unpack_from('<H', reply, 1)[0]
          assert length == min(capacity, 1)
          assert not any(reply[3 + length:-1])
          assert crc8(reply[:-1]) == reply[-1]

  def test_interrupted_frames(self):
    h = self.p._handle
    for length in (1, 6, 7, 8, 127, 255):
      with h.dev.acquire() as spi:
        spi.xfer2(self._request()[:length])
      self._ping(self.p)

      with h.dev.acquire() as spi:
        spi.xfer2(self._request())
        # Exercise an accepted, truncated reply rather than an ignored early poll.
        time.sleep(0.001)
        assert spi.xfer2(bytes(length))[0] == ACK
      self._ping(self.p)

  def test_repeated_discovery(self):
    assert self.p.health()['spi_error_count'] == 0
    expected = self.p._handle.get_protocol_version()
    for _ in range(100):
      assert self.p._handle.get_protocol_version() == expected
      self._ping(self.p)
    assert self.p.health()['spi_error_count'] == 0

  def test_discovery_after_reset(self):
    for _ in range(10):
      self.p.reset()
      assert self.p.health()['spi_error_count'] == 0

  def test_discovery_with_pending_reply(self):
    expected = self.p._handle.get_protocol_version()
    for _ in range(10):
      with self.p._handle.dev.acquire() as spi:
        spi.xfer2(self._request())
      assert self.p._handle.get_protocol_version() == expected
      self._ping(self.p)
    assert self.p.health()['spi_error_count'] == 0

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
