import time
import struct
import spidev
from functools import reduce

from binascii import hexlify

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
    self.spi.max_speed_hz = 10000000

  # helpers
  def _transfer(self, endpoint, data, max_rx_len=1000):
    #endpoint = 0xAB

    packet = struct.pack(">BBHH", SYNC, endpoint, len(data), max_rx_len)
    packet += bytes([reduce(lambda x, y: x^y, packet)])
    self.spi.writebytes(packet)

    print("header", hexlify(packet, sep=' '))

    dat = b"\x00"
    while dat[0] not in [ACK, NACK]:
      dat = self.spi.xfer(b"\x00")
      # time.sleep(0.1)
      print("RET", hex(dat[0]))

    if dat == NACK:
      raise Exception("Got NACK response")

    packet = bytes(data)
    packet += bytes([reduce(lambda x, y: x^y, packet)])
    self.spi.xfer(packet)

    print("data", hexlify(packet, sep=' '))

    dat = b"\x00"
    while dat[0] not in [ACK, NACK]:
      dat = self.spi.xfer(b"\x00")
      # time.sleep(0.00001)
      print("RET", hex(dat[0]))

    if dat == NACK:
      raise Exception("Got NACK response")

    print("reading response")
    response_len = struct.unpack(">H", bytes(self.spi.xfer(b"\x00" * 2)))[0]
    print(f"response len: {response_len}")

    dat = bytes(self.spi.xfer(b"\x00" * (response_len + 1)))
    # TODO: verify CRC    
    dat = dat[:-1]

    print("data", hexlify(dat, sep=' '))
    return dat

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