import struct
import spidev
import logging
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
    logging.debug(f"  checksum: {cksum}")
    return cksum == 0

  def _transfer(self, endpoint, data, max_rx_len=1000):
    logging.debug("\n\n")
    logging.debug(f"starting transfer: {endpoint=}, {max_rx_len=}")
    logging.debug("==============================================")

    for n in range(MAX_RETRY_COUNT):
      logging.debug(f"\ntry #{n+1}")
      try:
        logging.debug("- send header")
        packet = struct.pack("<BBHH", SYNC, endpoint, len(data), max_rx_len)
        packet += bytes([reduce(lambda x, y: x^y, packet) ^ CHECKSUM_START])
        self.spi.xfer2(packet)

        logging.debug("- waiting for ACK")
        # TODO: add timeout?
        dat = b"\x00"
        while dat[0] not in [HACK, NACK]:
          dat = self.spi.xfer2(b"\x12")

        if dat[0] == NACK:
          raise Exception("Got NACK response for header")

        # add checksum
        if len(data):
          packet = bytes(data)
          packet += bytes([reduce(lambda x, y: x^y, packet) ^ CHECKSUM_START])
        else:
          packet = bytes([CHECKSUM_START, ])

        logging.debug("- sending data")
        self.spi.xfer2(packet)

        logging.debug("- waiting for ACK")
        dat = b"\x00"
        while dat[0] not in [DACK, NACK]:
          dat = self.spi.xfer2(b"\xab")

        if dat[0] == NACK:
          raise Exception("Got NACK response for data")

        response_len_bytes = bytes(self.spi.xfer2(b"\x00" * 2))
        response_len = struct.unpack("<H", response_len_bytes)[0]

        logging.debug("- receiving response")
        dat = bytes(self.spi.xfer2(b"\x00" * (response_len + 1)))

        # verify checksum
        if not self._check_checksum([DACK, *response_len_bytes, *dat]):
          raise Exception("SPI got bad checksum")

        return dat[:-1]
      except Exception as e:
        logging.exception(f"SPI transfer failed, {n} retries left")
    raise Exception(f"SPI transaction failed {MAX_RETRY_COUNT} times")

  # libusb1 functions
  def close(self):
    self.spi.close()

  def controlWrite(self, request_type, request, value, index, data, timeout=0):
    return self._transfer(0, struct.pack("<BHHH", request, value, index, 0))

  def controlRead(self, request_type, request, value, index, length, timeout=0):
    return self._transfer(0, struct.pack("<BHHH", request, value, index, length))

  # TODO: implement these
  def bulkWrite(self, endpoint, data, timeout=0):
    pass

  def bulkRead(self, endpoint, length, timeout=0):
    import math
    ret = []
    usb_max_size = 0x40
    tot = math.ceil(length / usb_max_size)
    for x in range(tot):
      d = self._transfer(endpoint, [], max_rx_len=usb_max_size)
      #print(len(d), usb_max_size, bytes(d))
      print(bytes(d))
      ret += d
      #print(f"{x+1}/{tot}, {len(ret)}")
      if len(d) < usb_max_size:
        break
    print("\n\n\nreturning", bytes(ret[:100]))
    return bytes(ret)
