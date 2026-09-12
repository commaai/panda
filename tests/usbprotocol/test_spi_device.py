import ctypes
import fcntl
import os
import tempfile
import unittest
from unittest.mock import patch

from panda import Panda
from panda.python.spi import PandaProtocolMismatch, PandaSpiHandle, PandaSpiUnavailable, SpiDevice


class TestSpiDevice(unittest.TestCase):
  def test_connection_ownership_and_ioctl_buffers(self):
    transfers = []

    def ioctl(fd, command, argument):
      if command == SpiDevice.SPI_IOC_RD_BITS_PER_WORD:
        return b'\x08'
      if command == SpiDevice.SPI_IOC_WR_MAX_SPEED_HZ:
        return 0
      tx, rx, length, speed, _, bits, *_ = SpiDevice.SPI_IOC_TRANSFER.unpack(argument)
      self.assertEqual((speed, bits), (SpiDevice.MAX_SPEED, 8))
      transfers.append(ctypes.string_at(tx, length))
      ctypes.memmove(rx, bytes(value ^ 0xff for value in transfers[-1]), length)
      return length

    with tempfile.NamedTemporaryFile() as file, patch('panda.python.spi.DEV_PATH', file.name), patch.object(fcntl, 'ioctl', ioctl):
      first = SpiDevice()
      try:
        with first.acquire() as spi:
          self.assertEqual(spi.xfer(b'\x01\x02'), b'\xfe\xfd')
        # The OS lock remains held between transfers, including same-process opens.
        with self.assertRaises(PandaSpiUnavailable):
          SpiDevice()
        self.assertEqual(transfers, [b'\x01\x02'])
        with first.acquire() as spi:
          self.assertEqual(bytes(spi.xfer2(b'\x03')), b'\xfc')
      finally:
        first.close()
      first.close()
      with self.assertRaises(PandaSpiUnavailable):
        with first.acquire():
          pass
      second = SpiDevice()
      second.close()

  def test_setup_failure_releases_ownership(self):
    with (tempfile.NamedTemporaryFile() as file, patch('panda.python.spi.DEV_PATH', file.name),
          patch.object(fcntl, 'ioctl', side_effect=OSError('setup failed'))):
      with self.assertRaises(OSError):
        SpiDevice()
      with open(file.name, 'r+b') as next_owner:
        fcntl.flock(next_owner, fcntl.LOCK_EX | fcntl.LOCK_NB)

  def test_legacy_reads_and_writes_remain_separate_syscalls(self):
    def ioctl(fd, command, argument):
      if command == SpiDevice.SPI_IOC_RD_BITS_PER_WORD:
        return b'\x08'
      self.assertEqual(command, SpiDevice.SPI_IOC_WR_MAX_SPEED_HZ)
      return 0

    with tempfile.NamedTemporaryFile() as file, patch('panda.python.spi.DEV_PATH', file.name), patch.object(fcntl, 'ioctl', ioctl):
      device = SpiDevice()
      try:
        with device.acquire() as spi:
          spi.writebytes(b'VERSION')
          os.lseek(spi.fileno(), 0, os.SEEK_SET)
          self.assertEqual(spi.readbytes(7), b'VERSION')
          with self.assertRaises(OSError):
            spi.readbytes(1)
          for length in (0, SpiDevice.MAX_TRANSFER_SIZE + 1):
            with self.assertRaises(ValueError):
              spi.readbytes(length)
            with self.assertRaises(ValueError):
              spi.xfer2(bytes(length))
      finally:
        device.close()

  def test_unknown_protocol_can_be_listed_but_not_used(self):
    descriptor = bytes(range(12)) + bytes([1, 0xcc, 0xff])
    serial = descriptor[:12].hex()
    with patch('panda.python.spi.SpiDevice') as device:
      handle = PandaSpiHandle()
      handle._accept_protocol_version(descriptor)
      self.assertEqual(handle.get_protocol_version(), descriptor)
      with patch('panda.python.PandaSpiHandle', return_value=handle):
        self.assertEqual(Panda.spi_list(), [serial])
        with self.assertRaises(PandaProtocolMismatch):
          Panda.spi_connect(serial)
      self.assertEqual(device.return_value.close.call_count, 2)
      with self.assertRaises(PandaProtocolMismatch):
        handle.controlRead(0, 0xd2, 0, 0, 1)
      device.return_value.xfer2.assert_not_called()

  def test_supported_protocol_negotiation(self):
    for version in (2, 3):
      with self.subTest(version=version), patch('panda.python.spi.SpiDevice'):
        handle = PandaSpiHandle()
        descriptor = bytes(14) + bytes([version])
        self.assertEqual(handle._accept_protocol_version(descriptor), descriptor)
        self.assertEqual(handle.PROTOCOL_VERSION, version)
