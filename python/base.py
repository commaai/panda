from abc import ABC, abstractmethod
from typing import List, Optional


# This mimics the handle given by libusb1 for easy interoperability
class BaseHandle(ABC):
  @abstractmethod
  def close(self) -> None:
    ...

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
  @abstractmethod
  def close(self) -> None:
    ...

  @abstractmethod
  def program(self, address: int, dat: bytes, block_size: Optional[int] = None) -> None:
    ...

  @abstractmethod
  def erase(self, address: int) -> None:
    ...

  @abstractmethod
  def jump(self, address: int) -> None:
    ...

