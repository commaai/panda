import time
import struct
import spidev
from functools import reduce

# Constants
SYNC = 0x5A
ACK = 0x79
NACK = 0x1F

# This mimics the handle given by libusb1 for easy interoperability
class SpiHandle:
  def __init__(self):
    self.spi = spidev.SpiDev()
    self.spi.open(0, 0)

    # TODO: raise?
    self.spi.max_speed_hz = 1000000

  # helpers
  def _transfer(self, endpoint, data):
    packet = struct.pack("<BBHs", SYNC, endpoint, len(data), data)
    packet += bytes([reduce(lambda x, y: x^y, packet)])
    self.spi.writebytes(packet)

    dat = 0x00
    while dat not in [ACK, NACK]:
      dat = self.spi.xfer(b"\x00")
      time.sleep(1)
      print(dat)

    if dat == NACK:
      raise Exception("Got NACK response")

    print(dat)

  # libusb1 functions
  def close(self):
    self.spi.close()

  def controlWrite(self, request_type, request, value, index, data, timeout=0):
    return self._transfer(0, struct.pack("<HHHH", request, value, index, 0))

  def controlRead(self, request_type, request, value, index, length, timeout=0):
    return self._transfer(0, struct.pack("<HHHH", request, value, index, length))

  def bulkWrite(self, endpoint, data, timeout=0):
    pass

  def bulkRead(self, endpoint, data, timeout=0):
    pass