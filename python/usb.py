import struct
from typing import List, Optional

from .base import BaseHandle, BaseSTBootloaderHandle


class PandaUsbHandle(BaseHandle):
  def __init__(self, libusb_handle):
    self._libusb_handle = libusb_handle

  def close(self):
    self._libusb_handle.close()

  def controlWrite(self, request_type: int, request: int, value: int, index: int, data, timeout: int = 0):
    return self._libusb_handle.controlWrite(request_type, request, value, index, data, timeout)

  def controlRead(self, request_type: int, request: int, value: int, index: int, length: int, timeout: int = 0):
    return self._libusb_handle.controlRead(request_type, request, value, index, length, timeout)

  def bulkWrite(self, endpoint: int, data: List[int], timeout: int = 0) -> int:
    return self._libusb_handle.bulkWrite(endpoint, data, timeout)  # type: ignore

  def bulkRead(self, endpoint: int, length: int, timeout: int = 0) -> bytes:
    return self._libusb_handle.bulkRead(endpoint, length, timeout)  # type: ignore



class STBootloaderUSBHandle(BaseSTBootloaderHandle):
  DFU_DNLOAD = 1
  DFU_UPLOAD = 2
  DFU_GETSTATUS = 3
  DFU_CLRSTATUS = 4
  DFU_ABORT = 6

  def __init__(self, libusb_handle):
    self._libusb_handle = libusb_handle

  def _status(self) -> None:
    while 1:
      dat = self._libusb_handle.controlRead(0x21, self.DFU_GETSTATUS, 0, 0, 6)
      if dat[1] == 0:
        break

  def clear_status(self):
    # Clear status
    stat = self._libusb_handle.controlRead(0x21, self.DFU_GETSTATUS, 0, 0, 6)
    if stat[4] == 0xa:
      self._libusb_handle.controlRead(0x21, self.DFU_CLRSTATUS, 0, 0, 0)
    elif stat[4] == 0x9:
      self._libusb_handle.controlWrite(0x21, self.DFU_ABORT, 0, 0, b"")
      self._status()
    stat = str(self._libusb_handle.controlRead(0x21, self.DFU_GETSTATUS, 0, 0, 6))

  def close(self):
    self._libusb_handle.close()

  def program(self, address: int, dat: bytes, block_size: Optional[int] = None) -> None:
    if block_size is None:
      block_size = len(dat)

    # Set Address Pointer
    self._libusb_handle.controlWrite(0x21, self.DFU_DNLOAD, 0, 0, b"\x21" + struct.pack("I", address))
    self._status()

    # Program
    dat += b"\xFF" * ((block_size - len(dat)) % block_size)
    for i in range(0, len(dat) // block_size):
      ldat = dat[i * block_size:(i + 1) * block_size]
      print("programming %d with length %d" % (i, len(ldat)))
      self._libusb_handle.controlWrite(0x21, self.DFU_DNLOAD, 2 + i, 0, ldat)
      self._status()

  def erase(self, address):
    self._libusb_handle.controlWrite(0x21, self.DFU_DNLOAD, 0, 0, b"\x41" + struct.pack("I", address))
    self._status()

  def jump(self, address):
    self._libusb_handle.controlWrite(0x21, self.DFU_DNLOAD, 0, 0, b"\x21" + struct.pack("I", address))
    self._status()
    try:
      self._libusb_handle.controlWrite(0x21, self.DFU_DNLOAD, 2, 0, b"")
      _ = str(self._libusb_handle.controlRead(0x21, self.DFU_GETSTATUS, 0, 0, 6))
    except Exception:
      pass
