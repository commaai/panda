"""
Make a panda accessible via socketcan & python-can.

This allows using tools like wireshark, caringcaribou, etc. with panda,
which can be very useful when working with higher-level message protocols over CAN.

"""

from typing import Any, Optional, Tuple

from can.exceptions import error_check

import struct
import time
import logging
from collections import deque

import can
from can import Message, typechecking

logger = logging.getLogger(__name__)

from panda import Panda
from opendbc.car.can_definitions import CanData

# Signal to pycan to do software filtering because we haven't.
FILTERING_DONE = False

class PandaBus(can.BusABC):
  """python-can bus for a openpilot panda."""

  def __init__(
      self,
      channel: Any,
      can_filters: Optional[can.typechecking.CanFilters] = None,
      safety_mode: int = Panda.SAFETY_NOOUTPUT,
      safety_param: int = 0,
      **kwargs: object,
  ):
    """
    Construct and open a Panda instance.

    :param channel: Panda bus; called channel by super.
        Set to 3 to multiplex OBD.
    :param can_filters: From super.
    :param safety_mode: Panda safety mode.
    :param safety_param: Panda safety param.
    :param **kwargs: Passed to Panda().

    :raises ValueError: If parameters are out of range
    :raises ~can.exceptions.CanInterfaceNotImplementedError:
        If the driver cannot be accessed
    :raises ~can.exceptions.CanInitializationError:
        If the bus cannot be initialized
    """
    # Init Panda
    self.p = Panda(**kwargs)
    if channel == 3:
      self.p.set_obd(True)
      channel = 1
    self.bus = channel
    self.channel = channel
    self.p.can_clear(self.bus) # TX queue
    self.p.can_clear(0xFFFF) # RX queue
    self.p.set_safety_mode(safety_mode, safety_param)
    self._recv_buffer = deque()

    # Init for super.
    self._can_protocol = can.CanProtocol.CAN_FD
    self.channel_info = f"Panda: serial {self.p.get_serial()}, bus {self.bus}"
    # Must be run last.
    super().__init__(channel, can_filters=can_filters)

  def _recv_internal(
      self, timeout: Optional[float]
  ) -> Tuple[Optional[Message], bool]:
    """Block waiting for a message from the Bus.

    :param float timeout: Seconds to wait for a message.
                          None blocks indefinitely.
                          0.0 is non-blocking.

    :return:
        None on timeout or a Message object.
    :rtype: can.Message
    :raises can.interfaces.remote.protocol.RemoteError:
    """
    if not self._recv_buffer:
      recv = self.p.can_recv(timeout=pycan_timeout_to_panda(timeout))
      recv_this_bus = [r for r in recv if r[2] == self.bus]
      self._recv_buffer.extend(recv_this_bus)
    if not self._recv_buffer:
      return None, FILTERING_DONE
    candata = self._recv_buffer.popleft()
    msg = panda_to_pycan(candata)
    return msg, FILTERING_DONE

  def send(self, msg: Message, timeout: Optional[float] = None) -> None:
    """Transmit a message to the CAN bus.

    :param Message msg: A message object.

    :param timeout:
        If > 0, wait up to this many seconds for message to be ACK'ed or
        for transmit queue to be ready depending on driver implementation.
        If timeout is exceeded, an exception will be raised.
        Might not be supported by all interfaces.
        None blocks indefinitely.

    :raises ~can.exceptions.CanOperationError:
        If an error occurred while sending
    """
    addr, data, bus = pycan_to_panda(msg, self.bus)
    self.p.can_send(addr, data, bus,
                    fd=msg.is_fd,
                    timeout=pycan_timeout_to_panda(timeout))

  def shutdown(self):
    self.p.set_safety_mode(Panda.SAFETY_SILENT)
    self.p.close()
    # Must be run last.
    super().shutdown()

def panda_to_pycan(candata: CanData) -> Message:
  addr, data, bus = candata
  is_remote = False  # Not currently exposed by panda.
  return Message(channel=bus,
                 arbitration_id=addr,
                 is_extended_id=addr >= 0x800,
                 is_remote_frame=is_remote,
                 data=data,
                 is_fd=True,
                 timestamp=time.time(),
                 check=True)

def pycan_to_panda(msg: Message, bus) -> CanData:
  return msg.arbitration_id, msg.data, bus

def pycan_timeout_to_panda(timeout: Optional[float]) -> int:
  if timeout is None:
    # block forever
    return 0
  if timeout == 0:
    # non-blocking
    return -1
  else:
    # sec to millis
    return int(timeout*1e3)

# in_bus.recv => socketcan.send
# socketcan.recv => in_bus.send
def connect_to_socketcan(in_bus: can.Bus, socketcan_if='vcan0'):
  out_bus = can.Bus(channel=socketcan_if, interface='socketcan',
                    local_loopback=True, fd=True)
  abort = False
  next_in: Message
  next_out: Message
  # This isn't the best bidi message passing, but it'll do for now.
  while not abort:
    if next_in is None:
      next_in = in_bus.recv(timeout=0.0)
    if next_out is None:
      next_out = out_bus.recv(timeout=0.0)

    while next_in is not None and next_out is not None \
      and next_in.timestamp < next_out.timestamp:
        out_bus.send(next_in)
        next_in = in_bus.recv(timeout=0.0)
    while next_in is not None and next_out is not None \
      and next_in.timestamp > next_out.timestamp:
        in_bus.send(next_out)
        next_out = out_bus.recv(timeout=0.0)


if __name__ == '__main__':
    with PandaBus(interface="panda", channel = 1) as bus:

      connect_to_socketcan(bus)

        # bus.send(can.Message(
        #     arbitration_id=0x7DF,
        #     is_extended_id=False,
        #     channel=0,
        #     data=[0x02, 0x10, 0x03, ],
        #     dlc=3,
        # ))

      start = time.monotonic()
      while time.monotonic() - start < 10:
          _msg = bus.recv()
          print(_msg)



# install prereqs:
# pip install -e '.[dev]'


## mirror to socketcan:
# sudo modprobe vcan
# # Create a vcan network interface with a specific name
# sudo ip link add dev vcan0 type vcan
# sudo ip link set vcan0 up
# pac can-utils
# cansend vcan0 123#DEADBEEF

# export CAN_INTERFACE=socketcan
# export CAN_CHANNEL
# export CAN_BITRATE
# export CAN_CONFIG={"receive_own_messages": true, "fd": true}

