import binascii
import ctypes
import os
import math
import time
import struct
import threading
import secrets
import zlib
from contextlib import contextmanager
from functools import reduce

from .base import BaseHandle, BaseSTBootloaderHandle, TIMEOUT
from .constants import McuType, MCU_TYPE_BY_IDCODE, USBPACKET_MAX_SIZE
from .utils import logger

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


class SpiDevice:
  """One connection owns the SPI bus across every request and retry.

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
    self._lock = threading.RLock()
    self._file = None
    try:
      import fcntl
    except ImportError as e:
      raise PandaSpiUnavailable("SPI requires Linux") from e

    assert speed <= self.MAX_SPEED
    if not os.path.exists(DEV_PATH):
      raise PandaSpiUnavailable(f"SPI device not found: {DEV_PATH}")
    self._ioctl = fcntl.ioctl
    file = open(DEV_PATH, 'r+b', buffering=0)
    try:
      try:
        fcntl.flock(file.fileno(), fcntl.LOCK_EX | fcntl.LOCK_NB)
      except BlockingIOError as e:
        raise PandaSpiUnavailable("SPI device is already owned") from e
      self._ioctl(file.fileno(), self.SPI_IOC_WR_MAX_SPEED_HZ, struct.pack('=I', speed))
      bits = self._ioctl(file.fileno(), self.SPI_IOC_RD_BITS_PER_WORD, b'\x00')[0]
      self._fd = file.fileno()
      self._buffer = bytearray(self.MAX_TRANSFER_SIZE)
      self._view = memoryview(self._buffer)
      address = ctypes.addressof(ctypes.c_char.from_buffer(self._buffer))
      self._transfer = bytearray(self.SPI_IOC_TRANSFER.pack(address, address, 0, speed, 0, bits, 0, 0, 0, 0, 0))
      self._transfer_words = memoryview(self._transfer).cast('I')
      self._file = file
    except BaseException:
      file.close()
      raise

  @contextmanager
  def acquire(self):
    with self._lock:
      if self._file is None:
        raise PandaSpiUnavailable("SPI connection is closed")
      yield self

  def fileno(self) -> int:
    return self._fd

  def close(self):
    with self._lock:
      if self._file is not None:
        self._file.close()
        self._file = None

  def __del__(self):
    self.close()

  @classmethod
  def _check_length(cls, length: int):
    if not 0 < length <= cls.MAX_TRANSFER_SIZE:
      raise ValueError(f"SPI transfer length must be between 1 and {cls.MAX_TRANSFER_SIZE}")

  def xfer2(self, data) -> memoryview:
    length = len(data)
    self._check_length(length)
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
  FRAME_HEADER = struct.Struct("<BBHIQHBB")
  FRAME_OVERHEAD = 24
  MAX_PAYLOAD = SPI_BUF_SIZE - FRAME_OVERHEAD
  CAN_BATCH_RECORDS = 415  # smallest firmware TX queue's usable capacity
  CAN_DLC_LENGTHS = (0, 1, 2, 3, 4, 5, 6, 7, 8, 12, 16, 20, 24, 32, 48, 64)
  HEADER = struct.Struct("<BBHH")
  REQUEST_GAP = 500e-6
  RECOVERY_GAP = 250e-6
  CAN_RECORD_GAP = 4e-6
  CAN_READ_GAP = 750e-6
  MAINTENANCE_GAP = 0.1

  def __init__(self) -> None:
    self.no_retry = "NO_RETRY" in os.environ
    self._version = None
    self._writer_lock = threading.Lock()
    self._can_remaining = 0
    self._can_generation = 0
    self._can_pending = b""
    self._session = secrets.randbits(64) or 1
    self._seq = 0
    self._healthy = True
    self.dev = SpiDevice()

  def _fail(self, message):
    self._healthy = False
    raise PandaSpiTransferFailed(message) from None

  def _check_can_generation(self, generation):
    if generation is not None and generation != self._can_generation:
      self._fail("CAN stream reset during write; reconnect required")

  def _calc_checksum(self, data: bytes) -> int:
    cksum = CHECKSUM_START
    for b in data:
      cksum ^= b
    return cksum

  def _wait_for_ack(self, spi, ack_val: int, timeout: int, tx: int, length: int = 1) -> bytes:
    timeout_s = max(MIN_ACK_TIMEOUT_MS, timeout) * 1e-3

    start = time.monotonic()
    while (timeout == 0) or ((time.monotonic() - start) < timeout_s):
      dat = spi.xfer2([tx, ] * length)
      if dat[0] == ack_val:
        return bytes(dat)
      elif dat[0] == NACK:
        raise PandaSpiNackResponse

    raise PandaSpiMissingAck

  def _transfer_spidev(self, spi, endpoint: int, data, timeout: int, max_rx_len: int = 1000, expect_disconnect: bool = False) -> bytes:
    max_rx_len = max(USBPACKET_MAX_SIZE, max_rx_len)

    logger.debug("- send header")
    packet = self.HEADER.pack(SYNC, endpoint, len(data), max_rx_len)
    packet += bytes([self._calc_checksum(packet), ])
    spi.xfer2(packet)

    logger.debug("- waiting for header ACK")
    self._wait_for_ack(spi, HACK, MIN_ACK_TIMEOUT_MS, 0x11)

    logger.debug("- sending data")
    packet = bytes([*data, self._calc_checksum(data)])
    spi.xfer2(packet)

    if expect_disconnect:
      logger.debug("- expecting disconnect, returning")
      return b""
    else:
      logger.debug("- waiting for data ACK")
      preread_len = USBPACKET_MAX_SIZE + 1  # read enough for a controlRead
      dat = self._wait_for_ack(spi, DACK, timeout, 0x13, length=3 + preread_len)

      # get response length, then response
      response_len = struct.unpack("<H", dat[1:3])[0]
      if response_len > max_rx_len:
        raise PandaSpiException(f"response length greater than max ({max_rx_len} {response_len})")

      # read rest
      remaining = (response_len + 1) - preread_len
      if remaining > 0:
        dat += bytes(spi.readbytes(remaining))

      dat = dat[:3 + response_len + 1]
      if self._calc_checksum(dat) != 0:
        raise PandaSpiBadChecksum

      return dat[3:-1]

  def _transfer_legacy(self, endpoint: int, data, timeout: int, max_rx_len: int = 1000, expect_disconnect: bool = False) -> bytes:
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
          if self.no_retry:
            break

          # ensure slave is in a consistent state and ready for the next transfer
          # (e.g. slave TX buffer isn't stuck full)
          nack_cnt = 0
          attempts = 5
          while (nack_cnt <= 3) and (attempts > 0):
            attempts -= 1
            try:
              self._wait_for_ack(spi, NACK, MIN_ACK_TIMEOUT_MS, 0x11, length=XFER_SIZE//2)
              nack_cnt += 1
            except PandaSpiException:
              nack_cnt = 0

    raise exc

  def _accept_protocol_version(self, version):
    if len(version) < 15:
      raise PandaSpiException("SPI version response is too short")
    if version[14] == 2:
      self.PROTOCOL_VERSION = 2  # legacy firmware and bootstub use v2
    # Enumeration can identify unsupported firmware; transfers check compatibility.
    self._version = version
    return version

  def _legacy_protocol_version(self, spi):
    # Legacy bootstubs publish the prefix asynchronously, then wait for a separate
    # payload read. A single full-frame read cannot replace this exchange.
    spi.writebytes(b"VERSION")
    time.sleep(self.REQUEST_GAP)
    deadline = time.monotonic() + 0.1
    while time.monotonic() < deadline:
      prefix = bytes(spi.readbytes(9))
      time.sleep(self.RECOVERY_GAP)
      if len(prefix) != 9 or not prefix.startswith(b"VERSION"):
        continue
      length = struct.unpack_from("<H", prefix, 7)[0]
      if not 15 <= length <= 1000:
        raise PandaSpiException("invalid SPI version response length")
      payload = bytes(spi.readbytes(length + 1))
      time.sleep(self.RECOVERY_GAP)
      if len(payload) != length + 1 or crc8(prefix + payload[:-1]) != payload[-1]:
        raise PandaSpiBadChecksum
      return payload[:-1]
    raise PandaSpiMissingAck("legacy SPI version discovery timed out")

  def get_protocol_version(self) -> bytes:
    with self.dev.acquire() as spi:
      if self._version is not None:
        return self._version
      for _ in range(3):
        spi.xfer2(b"VERSION")
        time.sleep(self.REQUEST_GAP)
        packet = bytes(spi.xfer2(bytes(25)))
        time.sleep(self.RECOVERY_GAP)
        if len(packet) == 25 and packet[:9] == b"VERSION\x0f\x00" and crc8(packet[:-1]) == packet[-1]:
          return self._accept_protocol_version(packet[9:24])
      for _ in range(10):
        try:
          return self._accept_protocol_version(self._legacy_protocol_version(spi))
        except PandaSpiException:
          continue
      raise PandaSpiMissingAck("SPI version discovery failed")

  def _exchange_v3(self, spi, endpoint, payload, capacity, deadline, expect_disconnect=False):
    if self._seq > 0xffffffff:
      self._fail("SPI sequence exhausted; reconnect required")
    frame = self.FRAME_HEADER.pack(SYNC, endpoint, len(payload), self._seq, self._session, capacity, 3, 0) + payload
    frame += struct.pack("<I", zlib.crc32(frame))
    maintenance = endpoint == 2 or (endpoint == 0 and payload and payload[0] == 0xb5)
    gap = self.MAINTENANCE_GAP if maintenance else self.REQUEST_GAP
    if endpoint == 3:
      _, can_remaining, completed = self._can_chunk(payload, self._can_remaining, max_records=None)
      gap += completed * self.CAN_RECORD_GAP
    if endpoint in (1, 0x81) or (endpoint == 3 and capacity > 0):
      gap = max(gap, self.CAN_READ_GAP)
    if expect_disconnect:
      if time.monotonic() >= deadline:
        self._fail("SPI disconnect request deadline expired")
      try:
        spi.xfer2(frame)
        time.sleep(gap)
      finally:
        # Explicit reset API: outcome is intentionally unacknowledged, never replayed.
        self._healthy = False
      return b""
    attempted = False
    while time.monotonic() < deadline:
      try:
        attempted = True
        spi.xfer2(frame)
        time.sleep(gap)
        reply = bytes(spi.xfer2(bytes(self.FRAME_OVERHEAD + capacity)))
        time.sleep(self.RECOVERY_GAP)
      except OSError:
        # The command may already have executed: only retry its identical identity.
        time.sleep(max(gap, self.RECOVERY_GAP))
        reply = b""
      if len(reply) == self.FRAME_OVERHEAD + capacity and zlib.crc32(reply[:-4]) == struct.unpack("<I", reply[-4:])[0]:
        magic, status, length, seq, session, rxcap, version, reserved = self.FRAME_HEADER.unpack_from(reply)
        if magic == 0xa5 and version == 3 and reserved == 0 and session == self._session and seq == self._seq and rxcap == 0 and length <= capacity:
          if status in (2, 3):
            self._fail("SPI session or sequence lost; outcome unknown, reconnect required")
          if status in (0, 1):
            self._seq += 1
            if status == 0:
              if endpoint == 3:
                self._can_remaining = can_remaining
              return reply[self.FRAME_HEADER.size:self.FRAME_HEADER.size + length]
            raise PandaSpiNackResponse("SPI operation rejected without effects")
      if self.no_retry:
        break
    if not attempted:
      if endpoint == 3 and self._can_remaining:
        self._fail("CAN write deadline abandoned a partial record; reconnect required")
      raise PandaSpiNackResponse("SPI deadline expired before request; no effects")
    self._fail("SPI operation timed out; outcome unknown, reconnect required")

  def _transfer(self, endpoint: int, data, timeout: int, max_rx_len: int = 0, expect_disconnect: bool = False, can_generation=None) -> bytes:
    payload = bytes(data)
    if len(payload) > self.MAX_PAYLOAD or not 0 <= max_rx_len <= self.MAX_PAYLOAD:
      raise ValueError("SPI frame exceeds maximum payload")
    deadline = time.monotonic() + timeout * 1e-3 if timeout else math.inf
    while True:
      with self.dev.acquire() as spi:
        if not self._healthy:
          raise PandaSpiTransferFailed("SPI session invalid; reconnect required")
        if endpoint == 3 and can_generation is None:
          can_generation = self._can_generation
        self._check_can_generation(can_generation)
        version = self.get_protocol_version()
        if version[14] not in (2, 3):
          raise PandaProtocolMismatch(f"unsupported SPI protocol {version[14]}")
        if self.PROTOCOL_VERSION == 2:
          return self._transfer_legacy(endpoint, payload, timeout, max_rx_len, expect_disconnect)
        if self._seq == 0:
          self._exchange_v3(spi, 0xfe, b"", 0, deadline)
          self._can_remaining = 0
          self._can_pending = b""
        if endpoint in (1, 0x81) and self._can_pending:
          result = self._can_pending[:max_rx_len]
          self._can_pending = self._can_pending[len(result):]
          return result
        capacity = max_rx_len
        if endpoint == 3:
          capacity = 0 if self._can_pending else self.MAX_PAYLOAD
        try:
          result = self._exchange_v3(spi, endpoint, payload, capacity, deadline, expect_disconnect)
          if endpoint == 3:
            if result:
              self._can_pending = result
            result = b""
          if endpoint == 0 and payload and payload[0] == 0xc0:
            self._can_remaining = 0
            self._can_pending = b""
            self._can_generation += 1
          return result
        except PandaSpiNackResponse:
          if self._seq > 0xffffffff:
            self._fail("SPI sequence exhausted; reconnect required")
          if endpoint != 3 or self.no_retry or time.monotonic() >= deadline:
            if endpoint == 3 and self._can_remaining:
              self._fail("CAN write rejection abandoned a partial record; reconnect required")
            raise
      # Terminal rejection proves zero effects. Let CAN reads free receipt capacity
      # before allocating another identity; unresolved transport retries stay locked.
      time.sleep(self.RECOVERY_GAP)

  # libusb1 functions
  def close(self):
    self.dev.close()

  def controlWrite(self, request_type: int, request: int, value: int, index: int, data, timeout: int = TIMEOUT, expect_disconnect: bool = False):
    return self._transfer(0, struct.pack("<BHHH", request, value, index, 0), timeout, expect_disconnect=expect_disconnect)

  def controlRead(self, request_type: int, request: int, value: int, index: int, length: int, timeout: int = TIMEOUT):
    return self._transfer(0, struct.pack("<BHHH", request, value, index, length), timeout, max_rx_len=length)

  def _can_chunk(self, data, remaining, max_records=CAN_BATCH_RECORDS):
    """Count completed records and trailing bytes, optionally limiting admission."""
    offset = completed = 0
    limit = min(len(data), self.MAX_PAYLOAD)
    while offset < limit and (max_records is None or completed < max_records):
      if remaining == 0:
        remaining = 6 + self.CAN_DLC_LENGTHS[data[offset] >> 4]
      consumed = min(remaining, limit - offset)
      offset += consumed
      remaining -= consumed
      completed += remaining == 0
    return offset, remaining, completed

  def bulkWrite(self, endpoint: int, data: bytes, timeout: int = TIMEOUT) -> int:
    with self._writer_lock:
      self.get_protocol_version()
      chunk_size = self.MAX_PAYLOAD if self.PROTOCOL_VERSION == 3 else XFER_SIZE
      mv = memoryview(data)
      if endpoint == 3 and self.PROTOCOL_VERSION == 3:
        with self.dev.acquire():
          generation = self._can_generation
        offset = 0
        while offset < len(mv):
          with self.dev.acquire():
            self._check_can_generation(generation)
            size, _, _ = self._can_chunk(mv[offset:], self._can_remaining)
          self._transfer(endpoint, mv[offset:offset + size], timeout, can_generation=generation)
          with self.dev.acquire():
            self._check_can_generation(generation)
          offset += size
      else:
        for x in range(math.ceil(len(data) / chunk_size)):
          self._transfer(endpoint, mv[chunk_size*x:chunk_size*(x+1)], timeout)
      return len(data)

  def bulkRead(self, endpoint: int, length: int, timeout: int = TIMEOUT) -> bytes:
    self.get_protocol_version()
    chunk_size = self.MAX_PAYLOAD if self.PROTOCOL_VERSION == 3 else XFER_SIZE
    ret = b""
    for _ in range(math.ceil(length / chunk_size)):
      capacity = min(chunk_size, length - len(ret))
      d = self._transfer(endpoint, [], timeout, max_rx_len=capacity)
      ret += d
      if len(d) < capacity:
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
    except Exception:
      self.dev.close()
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
