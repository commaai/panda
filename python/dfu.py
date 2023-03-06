import usb1
import struct
import binascii
from typing import List, Tuple, Optional

from .base import BaseSTBootloaderHandle
from .usb import STBootloaderUSBHandle
from .constants import McuType


class PandaDFU:
  def __init__(self, dfu_serial: Optional[str]):
    # try USB, then SPI
    handle, mcu_type = PandaDFU.usb_connect(dfu_serial)
    if None in (handle, mcu_type):
      handle, mcu_type = PandaDFU.spi_connect(dfu_serial)

    if handle is None or mcu_type is None:
      raise Exception(f"failed to open DFU device {dfu_serial}")

    self._handle: BaseSTBootloaderHandle = handle
    self._mcu_type: McuType = mcu_type

  @staticmethod
  def usb_connect(dfu_serial: Optional[str]) -> Tuple[Optional[BaseSTBootloaderHandle], Optional[McuType]]:
    handle, mcu_type = None, None
    context = usb1.USBContext()
    for device in context.getDeviceList(skip_on_error=True):
      if device.getVendorID() == 0x0483 and device.getProductID() == 0xdf11:
        try:
          this_dfu_serial = device.open().getASCIIStringDescriptor(3)
        except Exception:
          continue
        if this_dfu_serial == dfu_serial or dfu_serial is None:
          handle = STBootloaderUSBHandle(device.open())
          # TODO: Find a way to detect F4 vs F2
          # TODO: also check F4 BCD, don't assume in else
          mcu_type = McuType.H7 if device.getbcdDevice() == 512 else McuType.F4
          break

    return handle, mcu_type

  @staticmethod
  def spi_connect(dfu_serial: Optional[str]) -> Tuple[Optional[BaseSTBootloaderHandle], Optional[McuType]]:
    return None, None

  @staticmethod
  def list() -> List[str]:
    ret = PandaDFU.usb_list()
    ret += PandaDFU.spi_list()
    return list(set(ret))

  @staticmethod
  def usb_list() -> List[str]:
    context = usb1.USBContext()
    dfu_serials = []
    try:
      for device in context.getDeviceList(skip_on_error=True):
        if device.getVendorID() == 0x0483 and device.getProductID() == 0xdf11:
          try:
            dfu_serials.append(device.open().getASCIIStringDescriptor(3))
          except Exception:
            pass
    except Exception:
      pass
    return dfu_serials

  @staticmethod
  def spi_list() -> List[str]:
    return []

  @staticmethod
  def st_serial_to_dfu_serial(st: str, mcu_type: McuType = McuType.F4):
    if st is None or st == "none":
      return None
    uid_base = struct.unpack("H" * 6, bytes.fromhex(st))
    if mcu_type == McuType.H7:
      return binascii.hexlify(struct.pack("!HHH", uid_base[1] + uid_base[5], uid_base[0] + uid_base[4], uid_base[3])).upper().decode("utf-8")
    else:
      return binascii.hexlify(struct.pack("!HHH", uid_base[1] + uid_base[5], uid_base[0] + uid_base[4] + 0xA, uid_base[3])).upper().decode("utf-8")

  def get_mcu_type(self) -> McuType:
    return self._mcu_type

  def erase(self, address: int) -> None:
    self._handle.erase(address)

  def reset(self):
    self._handle.jump(self._mcu_type.config.bootstub_address)

  def program_bootstub(self, code_bootstub):
    self._handle.clear_status()
    self.erase(self._mcu_type.config.bootstub_address)
    self.erase(self._mcu_type.config.app_address)
    self._handle.program(self._mcu_type.config.bootstub_address, code_bootstub, self._mcu_type.config.block_size)
    self.reset()

  def recover(self):
    with open(self._mcu_type.config.bootstub_path, "rb") as f:
      code = f.read()
    self.program_bootstub(code)

