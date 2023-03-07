from abc import ABC, abstractmethod
from typing import List

from .constants import McuType


class BaseHandle(ABC):
  """
    A handle to talk to a panda.
    Borrows heavily from the libusb1 handle API.
  """
  @abstractmethod
  def close(self) -> None:
    ...

  # TODO: enforce a timeout for all comms to ensure USB and SPI behavior match

  @abstractmethod
  def controlWrite(self, request_type: int, request: int, value: int, index: int, data, timeout: int = 0) -> int:
    ...

  @abstractmethod
  def controlRead(self, request_type: int, request: int, value: int, index: int, length: int, timeout: int = 0) -> bytes:
    ...

  @abstractmethod
  def bulkWrite(self, endpoint: int, data: List[int], timeout: int = 0) -> int:
    ...

  @abstractmethod
  def bulkRead(self, endpoint: int, length: int, timeout: int = 0) -> bytes:
    ...


class BaseSTBootloaderHandle(ABC):
  """
    A handle to talk to a panda while it's in the STM32 bootloader.
  """

  @abstractmethod
  def get_mcu_type(self) -> McuType:
    ...

  @abstractmethod
  def close(self) -> None:
    ...

  @abstractmethod
  def clear_status(self) -> None:
    ...

  @abstractmethod
  def program(self, address: int, dat: bytes) -> None:
    ...

  @abstractmethod
  def erase_app(self) -> None:
    ...

  @abstractmethod
  def erase_bootstub(self) -> None:
    ...

  @abstractmethod
  def jump(self, address: int) -> None:
    ...

