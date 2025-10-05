import binascii
import os
import fcntl
import math
import time
import struct
import threading
from contextlib import contextmanager
from functools import reduce

from .base import BaseHandle, BaseSTBootloaderHandle, TIMEOUT
from .constants import McuType, MCU_TYPE_BY_IDCODE, USBPACKET_MAX_SIZE
from .utils import logger

try:
  import spidev
except ImportError:
  spidev = None

# Constants
SYNC = 0x5A
HACK = 0x79
DACK = 0x85
NACK = 0x1F
CHECKSUM_START = 0xAB

MIN_ACK_TIMEOUT_MS = 100
MAX_XFER_RETRY_COUNT = 5

SPI_BUF_SIZE = 4096  # from panda/board/drivers/spi.h
XFER_SIZE = SPI_BUF_SIZE - 0x40 # give some room for SPI protocol overhead

DEV_PATH = "/dev/spidev0.0"


def crc8(data):
  crc = 0xFF    # standard init value
  poly = 0xD5   # standard crc8: x8+x7+x6+x4+x2+1
  size = len(data)
  for i in range(size - 1, -1, -1):
    crc ^= data[i]
    for _ in range(8):
      if ((crc & 0x80) != 0):
        crc = ((crc << 1) ^ poly) & 0xFF
      else:
        crc <<= 1
  return crc


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
  """
  Provides locked, thread-safe access to a panda's SPI interface.
  """

  MAX_SPEED = 50000000  # max of the SDM845

  def __init__(self, speed=MAX_SPEED):
    assert speed <= self.MAX_SPEED

    if not os.path.exists(DEV_PATH):
      raise PandaSpiUnavailable(f"SPI device not found: {DEV_PATH}")
    if spidev is None:
      raise PandaSpiUnavailable("spidev is not installed")

    with SPI_LOCK:
      if speed not in SPI_DEVICES:
        SPI_DEVICES[speed] = spidev.SpiDev()
        SPI_DEVICES[speed].open(0, 0)
        SPI_DEVICES[speed].max_speed_hz = speed
      self._spidev = SPI_DEVICES[speed]

  @contextmanager
  def acquire(self):
    try:
      SPI_LOCK.acquire()
      fcntl.flock(self._spidev, fcntl.LOCK_EX)
      yield self._spidev
    finally:
      fcntl.flock(self._spidev, fcntl.LOCK_UN)
      SPI_LOCK.release()

  def close(self):
    pass


class PandaSpiHandle(BaseHandle):
  """
  A class that mimics a libusb1 handle for panda SPI communications.
  """

  PROTOCOL_VERSION = 2
  HEADER = struct.Struct("<BBHH")

  def __init__(self) -> None:
    self.dev = SpiDevice()

  # helpers
  def _calc_checksum(self, data: bytes) -> int:
    cksum = CHECKSUM_START
    for b in data:
      cksum ^= b
    return cksum

  def _transfer_spidev(self, spi, endpoint: int, data, timeout: int, max_rx_len: int = 1000, expect_disconnect: bool = False) -> bytes:
    max_rx_len = max(USBPACKET_MAX_SIZE, max_rx_len)

    # *** TX to panda ***
    frame = bytearray(SPI_BUF_SIZE)
    frame[0:self.HEADER.size] = self.HEADER.pack(SYNC, endpoint, len(data), max_rx_len)
    frame[self.HEADER.size] = self._calc_checksum(frame[:self.HEADER.size])
    data_offset = self.HEADER.size + 1
    if len(data) > 0:
      frame[data_offset:data_offset+len(data)] = data
    frame[data_offset + len(data)] = self._calc_checksum(data)
    spi.xfer2(frame)

    # *** RX from panda ***
    if expect_disconnect:
      logger.debug("- expecting disconnect, returning")
      return b""

    deadline = time.monotonic() + (timeout * 1e-3 if timeout > 0 else 3600.0)

    status = 0
    while status != DACK:
      response = spi.readbytes(SPI_BUF_SIZE)
      status = response[0]
      if status == NACK:
        raise PandaSpiNackResponse
      if timeout != 0 and time.monotonic() > deadline:
        raise PandaSpiMissingAck

    len_bytes = response[1:3]
    response_len = struct.unpack('<H', bytes(len_bytes))[0]
    if response_len > max_rx_len:
      raise PandaSpiException(f"response length greater than max ({max_rx_len} {response_len})")

    data_ck = response[3:3+response_len+1] #spi.xfer2([0x00] * (response_len + 1))
    frame_head = bytes([DACK]) + bytes(len_bytes) + bytes(data_ck)
    if self._calc_checksum(frame_head) != 0:
      raise PandaSpiBadChecksum

    return bytes(data_ck[:-1])

  def _transfer(self, endpoint: int, data, timeout: int, max_rx_len: int = 1000, expect_disconnect: bool = False) -> bytes:
    logger.debug("starting transfer: endpoint=%d, max_rx_len=%d", endpoint, max_rx_len)
    logger.debug("==============================================")

    n = 0
    start_time = time.monotonic()
    exc = PandaSpiException()
    while (timeout == 0) or (time.monotonic() - start_time) < timeout*1e-3:
      n += 1
      logger.debug("\ntry #%d", n)
      with self.dev.acquire() as spi:
        try:
          return self._transfer_spidev(spi, endpoint, data, timeout, max_rx_len, expect_disconnect)
        except PandaSpiException as e:
          exc = e
          logger.debug("SPI transfer failed, retrying", exc_info=True)
          self._resync(spi)
    raise exc

  def _resync(self, spi):
    # ensure slave is in a consistent state and ready for the next transfer
    # (e.g. slave isn't stuck trying to RX/TX a massive buffer)
    attempts = 5
    while attempts > 0:
      x = spi.xfer2([0x00, ]*SPI_BUF_SIZE)
      if x[0] == NACK and set(bytes(x[1:])) == {0xcc, }:
        break
      attempts -= 1

  def get_protocol_version(self) -> bytes:
    vers_str = b"VERSION"

    def _get_version(spi) -> bytes:
      tx = bytearray(SPI_BUF_SIZE)
      tx[:len(vers_str)] = vers_str
      a = spi.xfer2(list(tx))

      response = spi.xfer2([0x00, ]*SPI_BUF_SIZE)
      if not bytes(response).startswith(vers_str):
        raise PandaSpiMissingAck

      len_bytes = bytes(response[7:7+2])
      rlen = struct.unpack('<H', len_bytes)[0]
      if rlen > 1000:
        raise PandaSpiException("response length greater than max")

      data_ck = response[9:9+rlen+1]
      if crc8(vers_str + len_bytes + bytes(data_ck[:-1])) != data_ck[-1]:
        raise PandaSpiBadChecksum

      return bytes(data_ck[:-1])

    exc = PandaSpiException()
    with self.dev.acquire() as spi:
      for _ in range(10):
        try:
          return _get_version(spi)
        except PandaSpiException as e:
          exc = e
          logger.debug("SPI get protocol version failed, retrying", exc_info=True)
          self._resync(spi)

    raise exc

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
        import traceback
        traceback.print_exception(type(e), e, e.__traceback__, limit=None)
        print()
        print()
        print()
        print()
        #logger.debug("SPI transfer failed, %d retries left", MAX_XFER_RETRY_COUNT - n - 1, exc_info=True)
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
