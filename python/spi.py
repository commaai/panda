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
HACK = 0x79
DACK = 0x85
NACK = 0x1F
CHECKSUM_START = 0xAB

XFER_SIZE = 0x40
#XFER_SIZE = 1024
ACK_TIMEOUT_SECONDS = 0.1
MAX_XFER_RETRY_COUNT = 5

USB_MAX_SIZE = 0x40


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
      rx_buf = self.spi.xfer2(b'\x00'*XFER_SIZE)
    else:
      rx_buf = bytearray(rx_len)
      transfer_list = spidev2.SPITransferList((
        {
          'tx_buf': tx_data,
          'delay_usecs': 100,
        },
        {
          'rx_buf': rx_buf,
        },
      ))
      self.spi.submitTransferList(transfer_list)

    ack = rx_buf[0]
    response_len = struct.unpack("<H", bytes(rx_buf[1:3]))[0]

    #logging.debug("* response")
    #logging.debug("  * length: %d", response_len)
    #logging.debug("  * buf length: %d", len(rx_buf))
    #logging.debug("  * ACK: %s %s", repr(rx_buf[0]), hex(rx_buf[0]))
    #logging.debug("  * raw: %s", repr(rx_buf))

    if ack == NACK:
      raise PandaSpiNackResponse
    elif ack != DACK:
      raise PandaSpiMissingAck

    dat = rx_buf[3:3+response_len+1]
    if self._calc_checksum(rx_buf[:1+2+response_len+1]) != 0:
      raise PandaSpiBadChecksum

    return bytes(dat[:-1])

  def _transfer_retry(self, endpoint: int, data, rx_len: int = 0x40) -> bytes:
    #logging.debug("starting transfer: endpoint=%d, rx_len=%d", endpoint, rx_len)
    #logging.debug("==============================================")

    # build the packet
    header = struct.pack("<BBHH", SYNC, endpoint, len(data), rx_len)
    packet = header + bytes(data)
    packet += bytes([reduce(lambda x, y: x^y, packet) ^ CHECKSUM_START])
    assert len(packet) < XFER_SIZE
    #logging.debug("- header %s", repr(header))
    #logging.debug("- data %s", repr(data))
    #logging.debug("- checksum %s", repr(packet[-1]))
    #logging.debug("- length %d", len(packet))

    packet = packet.ljust(XFER_SIZE, b'\x00')

    for n in range(MAX_XFER_RETRY_COUNT):
      #logging.debug("\ntry #%d", n+1)
      try:
        return self._transfer(packet, rx_len)
      except PandaSpiException:
        #logging.exception("SPI transfer failed, %d retries left", n)
        pass
    raise PandaSpiTransferFailed(f"SPI message failed {MAX_XFER_RETRY_COUNT} times")

  # libusb1 functions
  def close(self):
    self.spi.close()

  def controlWrite(self, request_type: int, request: int, value: int, index: int, data, timeout: int = 0):
    return self._transfer_retry(0, struct.pack("<BHHH", request, value, index, 0))

  def controlRead(self, request_type: int, request: int, value: int, index: int, length: int, timeout: int = 0):
    return self._transfer_retry(0, struct.pack("<BHHH", request, value, index, length))

  # TODO: implement these properly
  def bulkWrite(self, endpoint: int, data: List[int], timeout: int = 0) -> int:
    #for x in range(math.ceil(len(data) / USB_MAX_SIZE)):
    #  self._transfer_retry(endpoint, data[USB_MAX_SIZE*x:USB_MAX_SIZE*(x+1)])
    return len(data)

  def bulkRead(self, endpoint: int, length: int, timeout: int = 0) -> bytes:
    return b"\x00"

    ret: List[int] = []
    for _ in range(math.ceil(length / USB_MAX_SIZE)):
      d = self._transfer_retry(endpoint, [], max_rx_len=USB_MAX_SIZE)
      ret += d
      if len(d) < USB_MAX_SIZE:
        break
    return bytes(ret)
