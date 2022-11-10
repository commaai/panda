import struct
import spidev
from functools import reduce
from typing import List

# Constants
SYNC = 0x5A
HACK = 0x79
DACK = 0x85
NACK = 0x1F
CHECKSUM_START = 0xAB

MAX_RETRY_COUNT = 5

# This mimics the handle given by libusb1 for easy interoperability
class SpiHandle:
  def __init__(self):
    self.spi = spidev.SpiDev()
    self.spi.open(0, 0)

    self.spi.max_speed_hz = 30000000

  # helpers
  def _check_checksum(self, data: List[int]) -> bool:
    cksum = CHECKSUM_START
    for b in data:
      cksum ^= b
    return cksum == 0

  def _transfer(self, endpoint, data, max_rx_len=1000):
    for _ in range(MAX_RETRY_COUNT):
      try:
        packet = struct.pack("<BBHH", SYNC, endpoint, len(data), max_rx_len)
        packet += bytes([reduce(lambda x, y: x^y, packet) ^ CHECKSUM_START])
        self.spi.xfer2(packet)

        # TODO: add timeout?
        dat = b"\x00"
        while dat[0] not in [HACK, NACK]:
          dat = self.spi.xfer2(b"\x12")

        if dat[0] == NACK:
          raise Exception("Got NACK response for header")

        # add checksum
        packet = bytes(data)
        packet += bytes([reduce(lambda x, y: x^y, packet) ^ CHECKSUM_START])
        self.spi.xfer2(packet)

        dat = b"\x00"
        while dat[0] not in [DACK, NACK]:
          dat = self.spi.xfer2(b"\xab")

        if dat[0] == NACK:
          raise Exception("Got NACK response for data")

        response_len_bytes = bytes(self.spi.xfer2(b"\x00" * 2))
        response_len = struct.unpack("<H", response_len_bytes)[0]

        dat = bytes(self.spi.xfer2(b"\x00" * (response_len + 1)))

        # verify checksum
        if not self._check_checksum([DACK, *response_len_bytes, *dat]):
          raise Exception("SPI got bad checksum")

        return dat[:-1]
      except Exception as e:
        print(str(e))
        pass
    raise Exception(f"SPI transaction failed {MAX_RETRY_COUNT} times")

  # libusb1 functions
  def close(self):
    self.spi.close()

  def controlWrite(self, request_type, request, value, index, data, timeout=0):
    return self._transfer(0, struct.pack("<HHHH", request, value, index, 0))

  def controlRead(self, request_type, request, value, index, length, timeout=0):
    return self._transfer(0, struct.pack("<HHHH", request, value, index, length))

  # TODO: implement these
  def bulkWrite(self, endpoint, data, timeout=0):
    pass

  def bulkRead(self, endpoint, data, timeout=0):
    pass
