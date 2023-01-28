import os
import time
import struct
import binascii
from functools import reduce

from .constants import McuType
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

    # TODO: get MCU type
    self._mcu_type = self.get_mcu_type()

  def _get_ack(self, timeout=1.0):
    data = 0x00
    start_time = time.monotonic()
    while not data in [ACK, NACK] and (time.monotonic() - start_time < timeout):
      data = self.spi.xfer([0x00, ])[0]
      time.sleep(0.001)
    self.spi.xfer([ACK, ])

    if data == NACK:
      raise Exception("Got NACK response")
    elif data != ACK:
      raise Exception(f"Missing ACK")

  def _cmd(self, cmd, data=None, read_bytes=0):
    # sync
    self.spi.xfer([SYNC, ])

    # send command
    self.spi.xfer([cmd, cmd ^ 0xFF])
    self._get_ack()

    # send data
    if data is not None:
      for d in data:
        self.spi.xfer(self.add_checksum(d))
        self._get_ack(timeout=20)

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


  def go_cmd(self, address: int):
    self._cmd(0x21, data=[struct.pack('>I', address), ])

  # ***** panda api *****

  def get_mcu_type(self) -> McuType:
    mcu_by_id = {mcu: mcu.config.mcu_idcode for mcu in McuType}
    return mcu_by_id.get(self.get_id())

  def global_erase(self):
    d = struct.pack('>H', 0xFFFF)
    self._cmd(0x44, data=[d, ])

  def program_bootstub(self):
    address = self._mcu_type.config.bootstub_address
    with open(self._mcu_type.config.bootstub_path, 'rb') as f:
      code = f.read()

    i = 0
    while i < len(code):
      print(i, len(code))
      block = code[i:i+256]
      if len(block) < 256:
        block += b'\xFF' * (256 - len(block))

      self._cmd(0x31, data=[
        struct.pack('>I', address + i),
        bytes([len(block) - 1]) + block,
      ])
      print(f"Written {len(block)} bytes to {hex(address + i)}")
      i += 256

  def reset(self):
    self.go_cmd(self._mcu_type.config.bootstub_address)
