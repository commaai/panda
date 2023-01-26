import os
import time
import struct
import binascii
from functools import reduce

from .spi import spidev, flocked, DEV_PATH

SYNC = 0x5A
ACK = 0x79
NACK = 0x1F

# https://www.st.com/resource/en/application_note/an4286-spi-protocol-used-in-the-stm32-bootloader-stmicroelectronics.pdf
class PandaSpiDFU:
  def __init__(self, dfu_serial):
    if not os.path.exists(DEV_PATH):
      raise PandaSpiUnavailable(f"SPI device not found: {DEV_PATH}")
    if spidev is None:
      raise PandaSpiUnavailable("spidev is not installed")

    self.spi = spidev.SpiDev()  # pylint: disable=c-extension-no-member
    self.spi.open(0, 0)
    self.spi.max_speed_hz = 1000000

    # say hello
    try:
      self.spi.xfer([SYNC, ])
      self._get_ack()
    except Exception:
      raise Exception("failed to connect to panda")
    print("all good")
    time.sleep(1.)

  def _get_ack(self):
    # TODO: add timeout
    data = 0x00
    while not data in [ACK, NACK]:
      data = self.spi.xfer([0x00, ])[0]
      time.sleep(0.001)
    self.spi.xfer([ACK, ])

    if data == NACK:
      raise Exception("Got NACK response")
    elif data != ACK:
      raise Exception(f"Got bad response: {data}")

  def _cmd(self, cmd, data=None, read_bytes=0):
    # sync
    self.spi.xfer([SYNC, ])

    # send command
    self.spi.xfer([cmd, cmd ^ 0xFF])
    self._get_ack()

    # send data
    if data is not None:
      self.spi.xfer(data)
      self._get_ack()

    # receive
    ret = None
    if read_bytes > 0:
      # send busy byte
      ret = self.spi.xfer([0x00, ]*(read_bytes + 1))[1:]
      self._get_ack()

    return ret

  def add_checksum(self, data):
    return data + bytes([reduce(lambda a, b: a ^ b, data)])

  # ***** ST Bootloader functions *****

  def get_bootloader_version(self):
    return self._cmd(0x01, read_bytes=1)[0]

  def get_id(self):
    ret = self._cmd(0x02, read_bytes=3)
    n, msb, lsb = ret
    assert n == 1
    return ((ret[1] << 8) + ret[2])

  def erase(self):
    self._cmd(0x44, data=[self.add_checksum(struct.pack('>H', 0xFFFF)), ])

