import struct
import unittest
from unittest.mock import Mock, patch

from panda import Panda
from panda.python.spi import (BUSY, ACK, FRAME_SIZE, MAX_PAYLOAD, NACK, SYNC, VERSION_ENDPOINT, PandaProtocolMismatch,
                              PandaSpiBadChecksum, PandaSpiBusy, PandaSpiException, PandaSpiHandle, PandaSpiNackResponse, crc8)


def response(data=b"", status=ACK):
  packet = bytearray(FRAME_SIZE)
  struct.pack_into("<BH", packet, 0, status, len(data))
  packet[3:3 + len(data)] = data
  packet[-1] = crc8(packet[:-1])
  return packet


class TestSpiProtocol(unittest.TestCase):
  def setUp(self):
    self.enterContext(patch('panda.python.spi.SpiDevice'))
    self.handle = PandaSpiHandle()
    self.spi = Mock()

  def test_fixed_request_and_response(self):
    self.spi.xfer2.side_effect = [bytes(FRAME_SIZE), response(b"abc")]
    self.assertEqual(self.handle._transfer_spidev(self.spi, 0, b"request", 100, 64), b"abc")
    request = self.spi.xfer2.call_args_list[0].args[0]
    self.assertEqual(len(request), FRAME_SIZE)
    self.assertEqual(request[:6], struct.pack("<BBHH", SYNC, 0, 7, 64))
    self.assertEqual(request[6:13], b"request")
    self.assertEqual(request[13:-1], bytes(FRAME_SIZE - 14))
    self.assertEqual(request[-1], crc8(request[:-1]))
    self.assertEqual(self.spi.xfer2.call_args_list[1].args[0], bytes(FRAME_SIZE))

  def test_early_polls(self):
    self.spi.xfer2.side_effect = [bytes(FRAME_SIZE)] * 4 + [response(b"ready")]
    self.assertEqual(self.handle._transfer_spidev(self.spi, 1, b"", 100), b"ready")
    self.assertEqual(self.spi.xfer2.call_count, 5)

  def test_discovery_uses_fixed_frames(self):
    identity = bytes(range(12)) + bytes([9, 0xcc, PandaSpiHandle.PROTOCOL_VERSION])
    self.handle.dev.acquire.return_value.__enter__.return_value = self.spi
    self.spi.xfer2.side_effect = [response(status=NACK), bytes(FRAME_SIZE), response(identity)]
    self.assertEqual(self.handle.get_protocol_version(), identity)
    frames = [call.args[0] for call in self.spi.xfer2.call_args_list]
    self.assertEqual([len(frame) for frame in frames], [FRAME_SIZE] * 3)
    self.assertEqual(frames[1][:6], struct.pack('<BBHH', SYNC, VERSION_ENDPOINT, 0, 15))

  def test_discovery_rejects_short_identity(self):
    with patch.object(self.handle, '_resync'), patch.object(self.handle, '_transfer_spidev', return_value=bytes(14)):
      with self.assertRaisesRegex(PandaSpiException, 'invalid discovery response length'):
        self.handle.get_protocol_version()

  def test_protocol_mismatch_in_app_and_bootstub(self):
    for pid in (0xcc, 0xee):
      with self.subTest(pid=pid):
        identity = bytes(range(12)) + bytes([9, pid, 2])
        with patch.object(PandaSpiHandle, 'get_protocol_version', return_value=identity):
          with self.assertRaises(PandaProtocolMismatch):
            Panda.spi_connect(None)
          self.assertIsNotNone(Panda.spi_connect(None, ignore_version=True)[2])

  def test_crc_covers_padding(self):
    packet = response(b"abc")
    packet[200] ^= 1
    self.spi.xfer2.side_effect = [bytes(FRAME_SIZE), packet]
    with self.assertRaises(PandaSpiBadChecksum):
      self.handle._transfer_spidev(self.spi, 1, b"", 100)

  def test_response_capacity(self):
    self.spi.xfer2.side_effect = [bytes(FRAME_SIZE), response(bytes(65))]
    with self.assertRaises(PandaSpiException):
      self.handle._transfer_spidev(self.spi, 1, b"", 100, 64)

  def test_request_capacity(self):
    with self.assertRaises(ValueError):
      self.handle._transfer_spidev(self.spi, 3, bytes(MAX_PAYLOAD + 1), 100)
    self.spi.xfer2.assert_not_called()

  def test_status(self):
    for status, error in ((NACK, PandaSpiNackResponse), (BUSY, PandaSpiBusy)):
      with self.subTest(status=status):
        self.spi.xfer2.side_effect = [bytes(FRAME_SIZE), response(status=status)]
        with self.assertRaises(error):
          self.handle._transfer_spidev(self.spi, 3, b"data", 100)

  def test_flash_chunk_alignment(self):
    data = bytes(range(256)) * 2
    with patch.object(self.handle, '_transfer', return_value=b"") as transfer:
      self.assertEqual(self.handle.bulkWrite(2, data), len(data))
    chunks = [bytes(call.args[1]) for call in transfer.call_args_list]
    self.assertEqual([len(chunk) for chunk in chunks], [248, 248, 16])
    self.assertEqual(b"".join(chunks), data)

  def test_backpressure_does_not_resync(self):
    self.handle.no_retry = True
    with patch.object(self.handle, '_transfer_spidev', side_effect=[PandaSpiBusy(), b""]) as transfer:
      with patch.object(self.handle, '_resync') as resync:
        self.assertEqual(self.handle._transfer(3, b"data", 100), b"")
    self.assertEqual(transfer.call_count, 2)
    resync.assert_not_called()

  def test_crc_matches_firmware_algorithm(self):
    for data in (b"VERSION", bytes(255), bytes(range(256))):
      crc = 0xFF
      for value in reversed(data):
        crc ^= value
        for _ in range(8):
          crc = ((crc << 1) ^ (0xD5 if crc & 0x80 else 0)) & 0xFF
      self.assertEqual(crc8(data), crc)
