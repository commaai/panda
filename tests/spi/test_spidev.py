import ctypes
import errno
import struct
from unittest.mock import Mock

import pytest

from panda.python import spi as spidev


@pytest.fixture
def device(monkeypatch, tmp_path):
  if spidev.fcntl is None:
    pytest.skip("fcntl is unavailable")
  node = tmp_path / 'spidev0.0'
  node.touch()
  ioctl = Mock(side_effect=lambda fd, request, data: b'\x08' if request == spidev.SPI_IOC_RD_BITS_PER_WORD else 0)
  monkeypatch.setattr(spidev.fcntl, 'ioctl', ioctl)
  dev = spidev.SpiDev(str(node), 50_000_000)
  assert ioctl.call_args_list[0].args[1:] == (0x40046B04, struct.pack('=I', 50_000_000))
  yield dev
  dev.close()


@pytest.mark.parametrize('length', [1, 2, 7, 68, 256, 4032, 4096])
@pytest.mark.parametrize('input_type', [bytes, bytearray, list, memoryview])
@pytest.mark.parametrize('method', ['xfer', 'xfer2'])
def test_transfer(device, monkeypatch, length, input_type, method):
  tx = bytes(i % 256 for i in range(length))
  rx = bytes(255 - b for b in tx)

  def ioctl(fd, request, descriptor):
    assert fd == device.fileno()
    assert request == 0x40206B00
    assert len(descriptor) == 32
    tx_addr, rx_addr, size, speed, delay, bits, cs, tx_nbits, rx_nbits, word_delay, pad = struct.unpack('=QQIIHBBBBBB', descriptor)
    assert (size, speed, delay, bits, cs, tx_nbits, rx_nbits, word_delay, pad) == (length, 50_000_000, 0, 8, 0, 0, 0, 0, 0)
    assert ctypes.string_at(tx_addr, size) == tx
    ctypes.memmove(rx_addr, rx, size)
    return size

  monkeypatch.setattr(spidev.fcntl, 'ioctl', ioctl)
  assert getattr(device, method)(input_type(tx)) == rx


def test_buffer_reuse(device, monkeypatch):
  results = []

  def ioctl(fd, request, descriptor):
    tx, rx, size = struct.unpack_from('=QQI', descriptor)
    results.append((tx, rx))
    ctypes.memset(rx, len(results), size)
    return size

  monkeypatch.setattr(spidev.fcntl, 'ioctl', ioctl)
  first = device.xfer(b'abc')
  assert device.xfer2(b'd') == b'\x02'
  assert first == b'\x01' * 3
  assert results[0] == results[1]


@pytest.mark.parametrize('length', [0, -1, 4097])
def test_invalid_lengths(device, length):
  with pytest.raises(ValueError):
    device.readbytes(length)
  for method in (device.xfer2, device.writebytes):
    with pytest.raises(ValueError):
      method(bytes(max(0, length)))


def test_read_write(device, monkeypatch):
  read = Mock(return_value=b'abc')
  write = Mock(return_value=3)
  monkeypatch.setattr(spidev.os, 'read', read)
  monkeypatch.setattr(spidev.os, 'write', write)
  assert device.readbytes(3) == b'abc'
  device.writebytes([97, 98, 99])
  read.assert_called_once_with(device.fileno(), 3)
  write.assert_called_once_with(device.fileno(), b'abc')


@pytest.mark.parametrize('method, syscall, argument', [('xfer2', 'ioctl', b'abc'), ('writebytes', 'write', b'abc'), ('readbytes', 'read', 3)])
def test_io_errors(device, monkeypatch, method, syscall, argument):
  target = spidev.fcntl if syscall == 'ioctl' else spidev.os
  short = b'a' if syscall == 'read' else 1
  monkeypatch.setattr(target, syscall, Mock(return_value=short))
  with pytest.raises(OSError, match='Short SPI'):
    getattr(device, method)(argument)
  monkeypatch.setattr(target, syscall, Mock(side_effect=OSError(errno.EIO, 'device error')))
  with pytest.raises(OSError) as exc:
    getattr(device, method)(argument)
  assert exc.value.errno == errno.EIO


def test_initialization_failure_closes_file(monkeypatch):
  if spidev.fcntl is None:
    pytest.skip("fcntl is unavailable")
  file = Mock()
  monkeypatch.setattr('builtins.open', Mock(return_value=file))
  monkeypatch.setattr(spidev.fcntl, 'ioctl', Mock(side_effect=OSError(errno.ENODEV, 'removed')))
  with pytest.raises(OSError):
    spidev.SpiDev('/dev/spidev0.0', 1_000_000)
  file.close.assert_called_once()


def test_close(device):
  device.close()
  device.close()
  with pytest.raises(ValueError):
    device.fileno()



def test_ack_copies_receive_buffer(device, monkeypatch):
  def ioctl(fd, request, descriptor):
    _, rx, size = struct.unpack_from('=QQI', descriptor)
    ctypes.memset(rx, spidev.HACK, size)
    return size

  monkeypatch.setattr(spidev.fcntl, 'ioctl', ioctl)
  handle = object.__new__(spidev.PandaSpiHandle)
  result = handle._wait_for_ack(device, spidev.HACK, 100, 0x11, length=3)
  assert isinstance(result, bytes)
  device._buffer[:3] = b'abc'
  assert result == bytes([spidev.HACK]) * 3


def test_transfer_after_close(device, monkeypatch):
  device.close()
  def ioctl(fd, request, descriptor):
    assert fd == -1
    raise OSError(errno.EBADF, 'closed')
  monkeypatch.setattr(spidev.fcntl, 'ioctl', ioctl)
  with pytest.raises(OSError):
    device.xfer2(b'abc')
