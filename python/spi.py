import binascii
import ctypes
import os
import math
import time
import struct
import threading
from contextlib import contextmanager
from functools import reduce

from .base import BaseHandle, BaseSTBootloaderHandle, TIMEOUT
from .constants import McuType, MCU_TYPE_BY_IDCODE, USBPACKET_MAX_SIZE
from .utils import logger

# Protocol, see board/drivers/spi.h
LISTENING = 0x3C
ACK = 0xC3
NACK = 0x5A
POLL = 0xFF
INFO_ENDPOINT = 0xF0

REQ_HEADER = struct.Struct("<BBHH")  # endpoint, seq, tx_len, max_rx_len
RESP_HEADER = struct.Struct("<BBH")  # status, seq, len
CRC = struct.Struct("<I")

MIN_TIMEOUT_MS = 100
MAX_XFER_RETRY_COUNT = 5  # ST bootloader

SPI_BUF_SIZE = 4096  # from panda/board/drivers/drivers.h
XFER_SIZE = SPI_BUF_SIZE - 0x40 # give some room for SPI protocol overhead

# first read of the response, enough for a controlRead
PREREAD_LEN = RESP_HEADER.size + USBPACKET_MAX_SIZE + CRC.size
POLL_PACKET = bytes([POLL, ]) * PREREAD_LEN

DEV_PATH = "/dev/spidev0.0"


class PandaSpiException(Exception):
  pass

class PandaProtocolMismatch(PandaSpiException):
  pass

class PandaSpiUnavailable(PandaSpiException):
  pass

class PandaSpiNackResponse(PandaSpiException):
  pass

class PandaSpiMissingAck(PandaSpiException):
  pass

class PandaSpiBadChecksum(PandaSpiException):
  pass

class PandaSpiTransferFailed(PandaSpiException):
  pass


SPI_LOCK = threading.Lock()
SPI_DEVICES = {}
class SpiDevice:
  """Provides locked, thread-safe access to a panda's SPI interface.

  xfer2 returns a view valid until the next transfer; xfer returns an owned copy.
  """

  MAX_SPEED = 50000000  # max of the SDM845

  # Linux asm-generic ioctl ABI (including aarch64 and x86_64), linux/spi/spidev.h.
  SPI_IOC_RD_BITS_PER_WORD = 0x80016B03
  SPI_IOC_WR_MAX_SPEED_HZ = 0x40046B04
  SPI_IOC_MESSAGE_1 = 0x40206B00
  SPI_IOC_TRANSFER = struct.Struct('=QQIIHBBBBBB')
  MAX_TRANSFER_SIZE = 4096

  def __init__(self, speed=MAX_SPEED):
    try:
      import fcntl
    except ImportError as e:
      raise PandaSpiUnavailable("SPI requires Linux") from e

    assert speed <= self.MAX_SPEED
    if not os.path.exists(DEV_PATH):
      raise PandaSpiUnavailable(f"SPI device not found: {DEV_PATH}")

    self._ioctl = fcntl.ioctl
    self._flock = fcntl.flock
    self._lock_ex, self._lock_un = fcntl.LOCK_EX, fcntl.LOCK_UN
    with SPI_LOCK:
      if speed not in SPI_DEVICES:
        file = open(DEV_PATH, 'r+b', buffering=0)
        try:
          self._ioctl(file.fileno(), self.SPI_IOC_WR_MAX_SPEED_HZ, struct.pack('=I', speed))
          bits = self._ioctl(file.fileno(), self.SPI_IOC_RD_BITS_PER_WORD, b'\x00')[0]
        except BaseException:
          file.close()
          raise
        SPI_DEVICES[speed] = (file, bits)
      self._file, bits = SPI_DEVICES[speed]

    self._fd = self._file.fileno()
    self._buffer = bytearray(self.MAX_TRANSFER_SIZE)
    self._view = memoryview(self._buffer)
    address = ctypes.addressof(ctypes.c_char.from_buffer(self._buffer))
    # The kernel supports using the same buffer for transmit and receive.
    self._transfer = bytearray(self.SPI_IOC_TRANSFER.pack(address, address, 0, speed, 0, bits, 0, 0, 0, 0, 0))
    self._transfer_words = memoryview(self._transfer).cast('I')

  @contextmanager
  def acquire(self):
    with SPI_LOCK:
      self._flock(self._fd, self._lock_ex)
      try:
        yield self
      finally:
        self._flock(self._fd, self._lock_un)

  def fileno(self) -> int:
    return self._fd

  def close(self):
    pass  # Connections are shared and cached by speed.

  @classmethod
  def _check_length(cls, length: int):
    if not 0 < length <= cls.MAX_TRANSFER_SIZE:
      raise ValueError(f"SPI transfer length must be between 1 and {cls.MAX_TRANSFER_SIZE}")

  def xfer2(self, data) -> memoryview:
    length = len(data)
    if not 0 < length <= self.MAX_TRANSFER_SIZE:
      raise ValueError(f"SPI transfer length must be between 1 and {self.MAX_TRANSFER_SIZE}")
    self._view[:length] = bytes(data)
    self._transfer_words[4] = length  # len field at byte offset 16
    if self._ioctl(self._fd, self.SPI_IOC_MESSAGE_1, self._transfer) != length:
      raise OSError("Short SPI transfer")
    return self._view[:length]

  # Both operations use one SPI_IOC_MESSAGE(1) for the single-block calls in panda.
  def xfer(self, data) -> bytes:
    return bytes(self.xfer2(data))

  def readbytes(self, length: int) -> bytes:
    self._check_length(length)
    data = os.read(self.fileno(), length)
    if len(data) != length:
      raise OSError("Short SPI read")
    return data

  def writebytes(self, data):
    data = bytes(data)
    self._check_length(len(data))
    if os.write(self.fileno(), data) != len(data):
      raise OSError("Short SPI write")

class PandaSpiHandle(BaseHandle):
  """
  A class that mimics a libusb1 handle for panda SPI communications.
  """

  PROTOCOL_VERSION = 3

  def __init__(self) -> None:
    self.dev = SpiDevice()
    self.no_retry = "NO_RETRY" in os.environ
    self.seq = 0

  def _transfer_spidev(self, spi, endpoint: int, data, deadline: float, max_rx_len: int, expect_disconnect: bool) -> bytes:
    self.seq = (self.seq + 1) & 0xFF
    packet = REQ_HEADER.pack(endpoint, self.seq, len(data), max_rx_len) + bytes(data)
    packet += CRC.pack(binascii.crc32(packet))

    while True:
      logger.debug("- sending request")
      while spi.xfer2(packet)[0] != LISTENING:
        # panda wasn't listening yet
        if time.monotonic() > deadline:
          raise PandaSpiMissingAck

      if expect_disconnect:
        logger.debug("- expecting disconnect, returning")
        return b""

      logger.debug("- waiting for response")
      while (dat := spi.xfer(POLL_PACKET))[0] not in (ACK, NACK, LISTENING):
        if time.monotonic() > deadline:
          raise PandaSpiMissingAck
      if dat[0] == LISTENING:
        logger.debug("- request was dropped, resending")
        if time.monotonic() > deadline:
          raise PandaSpiMissingAck
        continue

      status, seq, response_len = RESP_HEADER.unpack_from(dat)
      if response_len > max_rx_len:
        raise PandaSpiBadChecksum(f"response length greater than max ({max_rx_len} {response_len})")

      # read the rest
      total_len = RESP_HEADER.size + response_len + CRC.size
      if total_len > len(dat):
        dat += spi.xfer(bytes([POLL, ]) * (total_len - len(dat)))
      dat = dat[:total_len]

      if CRC.unpack_from(dat, total_len - CRC.size)[0] != binascii.crc32(dat[:-CRC.size]):
        raise PandaSpiBadChecksum
      if seq != self.seq:
        logger.debug("- got response to an old request, resending")
        if time.monotonic() > deadline:
          raise PandaSpiMissingAck
        continue
      if status == NACK:
        raise PandaSpiNackResponse
      return dat[RESP_HEADER.size:-CRC.size]

  def _transfer(self, endpoint: int, data, timeout: int, max_rx_len: int = 1000, expect_disconnect: bool = False) -> bytes:
    logger.debug("starting transfer: endpoint=%d, max_rx_len=%d", endpoint, max_rx_len)
    logger.debug("==============================================")

    max_rx_len = max(USBPACKET_MAX_SIZE, max_rx_len)
    deadline = time.monotonic() + (max(MIN_TIMEOUT_MS, timeout) * 1e-3 if timeout != 0 else math.inf)
    while True:
      with self.dev.acquire() as spi:
        try:
          return self._transfer_spidev(spi, endpoint, data, deadline, max_rx_len, expect_disconnect)
        except PandaSpiException:
          logger.debug("SPI transfer failed", exc_info=True)
          if self.no_retry or time.monotonic() > deadline:
            raise

  def get_protocol_version(self) -> bytes:
    # serial, HW type, bootstub, protocol version
    return self._transfer(INFO_ENDPOINT, b"", MIN_TIMEOUT_MS, max_rx_len=15)

  # libusb1 functions
  def close(self):
    self.dev.close()

  def controlWrite(self, request_type: int, request: int, value: int, index: int, data, timeout: int = TIMEOUT, expect_disconnect: bool = False):
    return self._transfer(0, struct.pack("<BHHH", request, value, index, 0), timeout, expect_disconnect=expect_disconnect)

  def controlRead(self, request_type: int, request: int, value: int, index: int, length: int, timeout: int = TIMEOUT):
    return self._transfer(0, struct.pack("<BHHH", request, value, index, length), timeout, max_rx_len=length)

  def bulkWrite(self, endpoint: int, data: bytes, timeout: int = TIMEOUT) -> int:
    mv = memoryview(data)
    for x in range(math.ceil(len(data) / XFER_SIZE)):
      self._transfer(endpoint, mv[XFER_SIZE*x:XFER_SIZE*(x+1)], timeout)
    return len(data)

  def bulkRead(self, endpoint: int, length: int, timeout: int = TIMEOUT) -> bytes:
    ret = b""
    for _ in range(math.ceil(length / XFER_SIZE)):
      d = self._transfer(endpoint, [], timeout, max_rx_len=XFER_SIZE)
      ret += d
      if len(d) < XFER_SIZE:
        break
    return ret


class STBootloaderSPIHandle(BaseSTBootloaderHandle):
  """
    Implementation of the STM32 SPI bootloader protocol described in:
    https://www.st.com/resource/en/application_note/an4286-spi-protocol-used-in-the-stm32-bootloader-stmicroelectronics.pdf

    NOTE: the bootloader's state machine is fragile and immediately gets into a bad state when
          sending any junk, e.g. when using the panda SPI protocol.
  """

  SYNC = 0x5A
  ACK = 0x79
  NACK = 0x1F

  def __init__(self):
    self.dev = SpiDevice(speed=1000000)

    # say hello
    try:
      with self.dev.acquire() as spi:
        spi.xfer([self.SYNC, ])
        try:
          self._get_ack(spi, 0.1)
        except (PandaSpiNackResponse, PandaSpiMissingAck):
          # NACK ok here, will only ACK the first time
          pass

      self._mcu_type = MCU_TYPE_BY_IDCODE[self.get_chip_id()]
    except PandaSpiException:
      raise PandaSpiException("failed to connect to panda") from None

  def _get_ack(self, spi, timeout=1.0):
    data = 0x00
    start_time = time.monotonic()
    while data not in (self.ACK, self.NACK) and (time.monotonic() - start_time < timeout):
      data = spi.xfer([0x00, ])[0]
      time.sleep(0)
    spi.xfer([self.ACK, ])

    if data == self.NACK:
      raise PandaSpiNackResponse
    elif data != self.ACK:
      raise PandaSpiMissingAck

  def _cmd_no_retry(self, cmd: int, data: list[bytes] | None = None, read_bytes: int = 0, predata=None) -> bytes:
    ret = b""
    with self.dev.acquire() as spi:
      # sync + command
      spi.xfer([self.SYNC, ])
      spi.xfer([cmd, cmd ^ 0xFF])
      self._get_ack(spi, timeout=0.01)

      # "predata" - for commands that send the first data without a checksum
      if predata is not None:
        spi.xfer(predata)
        self._get_ack(spi)

      # send data
      if data is not None:
        for d in data:
          if predata is not None:
            spi.xfer(d + self._checksum(predata + d))
          else:
            spi.xfer(d + self._checksum(d))
          self._get_ack(spi, timeout=20)

      # receive
      if read_bytes > 0:
        ret = spi.xfer([0x00, ]*(read_bytes + 1))[1:]
        if data is None or len(data) == 0:
          self._get_ack(spi)

    return bytes(ret)

  def _cmd(self, cmd: int, data: list[bytes] | None = None, read_bytes: int = 0, predata=None) -> bytes:
    exc = PandaSpiException()
    for n in range(MAX_XFER_RETRY_COUNT):
      try:
        return self._cmd_no_retry(cmd, data, read_bytes, predata)
      except PandaSpiException as e:
        exc = e
        logger.debug("SPI transfer failed, %d retries left", MAX_XFER_RETRY_COUNT - n - 1, exc_info=True)
    raise exc

  def _checksum(self, data: bytes) -> bytes:
    if len(data) == 1:
      ret = data[0] ^ 0xFF
    else:
      ret = reduce(lambda a, b: a ^ b, data)
    return bytes([ret, ])

  # *** Bootloader commands ***

  def read(self, address: int, length: int):
    data = [struct.pack('>I', address), struct.pack('B', length - 1)]
    return self._cmd(0x11, data=data, read_bytes=length)

  def get_bootloader_id(self):
    return self.read(0x1FF1E7FE, 1)

  def get_chip_id(self) -> int:
    r = self._cmd(0x02, read_bytes=3)
    if r[0] != 1: # response length - 1
      raise PandaSpiException("incorrect response length")
    return ((r[1] << 8) + r[2])

  def go_cmd(self, address: int) -> None:
    self._cmd(0x21, data=[struct.pack('>I', address), ])

  # *** helpers ***

  def get_uid(self):
    dat = self.read(McuType.H7.config.uid_address, 12)
    return binascii.hexlify(dat).decode()

  def erase_sector(self, sector: int):
    p = struct.pack('>H', 0)  # number of sectors to erase
    d = struct.pack('>H', sector)
    self._cmd(0x44, data=[d, ], predata=p)

  # *** PandaDFU API ***

  def get_mcu_type(self):
    return self._mcu_type

  def clear_status(self):
    pass

  def close(self):
    self.dev.close()

  def program(self, address, dat):
    bs = 256  # max block size for writing to flash over SPI
    dat += b"\xFF" * ((bs - len(dat)) % bs)
    for i in range(len(dat) // bs):
      block = dat[i * bs:(i + 1) * bs]
      self._cmd(0x31, data=[
        struct.pack('>I', address + i*bs),
        bytes([len(block) - 1]) + block,
      ])

  def jump(self, address):
    self.go_cmd(self._mcu_type.config.bootstub_address)
