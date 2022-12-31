import math
import time
import struct
import spidev
import logging
from functools import reduce
from typing import List

# TODO: remove this once spidev2 is in agnos
try:
  import spidev2
except ImportError:
  spidev2 = None

# Constants
SYNC = 0x5A
ACK = 0x85
NACK = 0x1F
CHECKSUM_START = 0xAB

BULK_XFER_SIZE = 1024
CONTROL_XFER_SIZE = 0x40  # same as USB packet max size
ACK_TIMEOUT_SECONDS = 0.1
MAX_XFER_RETRY_COUNT = 1


class PandaSpiException(Exception):
  pass

class PandaSpiNackResponse(PandaSpiException):
  pass

class PandaSpiMissingAck(PandaSpiException):
  pass

class PandaSpiBadChecksum(PandaSpiException):
  pass

class PandaSpiTransferFailed(PandaSpiException):
  pass


# This mimics the handle given by libusb1 for easy interoperability
class SpiHandle:
  def __init__(self):
    if spidev2 is not None:
      self.spi = spidev2.SPIBus('/dev/spidev0.0', 'w+b', bits_per_word=8, speed_hz=30_000_000, spi_mode=spidev2.SPIMode32.SPI_MODE_0)
    else:
      self.spi = spidev.SpiDev()  # pylint: disable=c-extension-no-member
      self.spi.open(0, 0)
      self.spi.max_speed_hz = 30_000_000

  # helpers
  def _calc_checksum(self, data: List[int]) -> int:
    cksum = CHECKSUM_START
    for b in data:
      cksum ^= b
    return cksum

  def _transfer(self, tx_data: bytes, rx_len: int) -> bytes:
    if isinstance(self.spi, spidev.SpiDev):
      self.spi.xfer2(tx_data)
      rx_buf = self.spi.xfer2(b'\x00'*rx_len)
    else:
      rx_buf = bytearray(rx_len)
      transfer_list = spidev2.SPITransferList((
        {
          'tx_buf': tx_data,
          'delay_usecs': 200,
        },
        {
          'rx_buf': rx_buf,
        },
      ))
      self.spi.submitTransferList(transfer_list)

    retry_count = 10
    while rx_buf[0] == 0x0 and retry_count > 0:
      print("trying again")
      time.sleep(0.01)
      self.spi.transfer(rx_buf=rx_buf)
      retry_count -= 1

    ack = rx_buf[0]
    response_len = struct.unpack("<H", bytes(rx_buf[1:3]))[0]

    logging.debug("* response")
    logging.debug("  * length: %d", response_len)
    logging.debug("  * buf length: %d", len(rx_buf))
    logging.debug("  * ACK: %s %s", repr(rx_buf[0]), hex(rx_buf[0]))
    logging.debug("  * raw: %s", repr(rx_buf))

    if ack == NACK:
      raise PandaSpiNackResponse
    elif ack != ACK:
      raise PandaSpiMissingAck

    dat = rx_buf[3:3+response_len+1]
    # TODO: this has a big perf hit
    if self._calc_checksum(rx_buf[:1+2+response_len+1]) != 0:
      raise PandaSpiBadChecksum

    return bytes(dat[:-1])

  def _transfer_retry(self, endpoint: int, data, rx_len: int = CONTROL_XFER_SIZE) -> bytes:
    logging.debug("starting transfer: endpoint=%d, rx_len=%d", endpoint, rx_len)
    logging.debug("==============================================")

    # build the packet
    header = struct.pack("<BBHH", SYNC, endpoint, len(data), rx_len)
    packet = header + bytes(data)
    packet += bytes([reduce(lambda x, y: x^y, packet) ^ CHECKSUM_START])
    logging.debug("- header %s", repr(header))
    logging.debug("- data %s", repr(data))
    logging.debug("- checksum %s", repr(packet[-1]))
    logging.debug("- length %d", len(packet))

    packet = packet.ljust(CONTROL_XFER_SIZE, b'\x00')

    exc = None
    for n in range(MAX_XFER_RETRY_COUNT):
      logging.debug("\ntry #%d", n+1)
      try:
        return self._transfer(packet, rx_len)
      except PandaSpiException as e:
        print("exception", type(e))
        exc = e
    raise exc

  # libusb1 functions
  def close(self):
    self.spi.close()

  def controlWrite(self, request_type: int, request: int, value: int, index: int, data, timeout: int = 0):
    dat = struct.pack("<BHHH", request, value, index, 0)
    return self._transfer_retry(0, dat)

  def controlRead(self, request_type: int, request: int, value: int, index: int, length: int, timeout: int = 0):
    dat = struct.pack("<BHHH", request, value, index, length)
    return self._transfer_retry(0, dat)

  # TODO: implement these properly
  def bulkWrite(self, endpoint: int, data: List[int], timeout: int = 0) -> int:
    xfer_size = CONTROL_XFER_SIZE - 6 - 1
    for x in range(math.ceil(len(data) / xfer_size)):
      self._transfer_retry(endpoint, data[xfer_size*x:xfer_size*(x+1)])
    return len(data)

  def bulkRead(self, endpoint: int, length: int, timeout: int = 0) -> bytes:
    ret: List[int] = []
    for _ in range(math.ceil(length / BULK_XFER_SIZE)):
      d = self._transfer_retry(endpoint, [], BULK_XFER_SIZE)
      ret += d
      if len(d) < BULK_XFER_SIZE:
        break
    return bytes(ret)
