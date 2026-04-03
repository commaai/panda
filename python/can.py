import struct
import signal

from .base import BaseHandle
from opendbc.car.isotp import isotp_send, isotp_recv


class CanHandle(BaseHandle):
  def __init__(self, p, bus):
    self.p = p
    self.bus = bus

  def transact(self, dat):
    def _handle_timeout(signum, frame):
      # will happen on reset or can error
      raise TimeoutError

    signal.signal(signal.SIGALRM, _handle_timeout)
    signal.alarm(1)

    try:
      isotp_send(self.p, dat, 1, self.bus, recvaddr=2)
    finally:
      signal.alarm(0)

    signal.signal(signal.SIGALRM, _handle_timeout)
    signal.alarm(1)
    try:
      ret = isotp_recv(self.p, 2, self.bus, sendaddr=1)
    finally:
      signal.alarm(0)

    return ret

  def close(self):
    pass

  def controlWrite(self, request_type, request, value, index, data, timeout=0, expect_disconnect=False):
    # ignore data in reply, panda doesn't use it
    return self.controlRead(request_type, request, value, index, 0, timeout)

  def controlRead(self, request_type, request, value, index, length, timeout=0):
    dat = struct.pack("HHBBHHH", 0, 0, request_type, request, value, index, length)
    return self.transact(dat)

  def bulkWrite(self, endpoint, data, timeout=0):
    dat = struct.pack("HH", endpoint, len(data)) + data
    return self.transact(dat)

  def bulkRead(self, endpoint, length, timeout=0):
    dat = struct.pack("HH", endpoint, 0)
    return self.transact(dat)