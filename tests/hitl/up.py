"""The HITL tests' stand-in for a panda jungle: an ultimate provisioning (UP) board.

The tests run on the DUT, and the DUT's internal panda is wired to the UP's OBD-C port.
`python -m ultimate_provisioning jungle --serve` runs on the UP and serves its harness and CAN
over the USB network between the UP and the DUT. Bus 3 is the OBD port, see ultimate_provisioning.
"""
import os
import xmlrpc.client

# the UP's address on its USB network to the DUT, see ultimate_provisioning
UP_IP = os.environ.get("UP_IP", "10.254.254.1")
PORT = 8722


class UPJungle:
  HARNESS_ORIENTATION_NONE = 0
  HARNESS_ORIENTATION_1 = 1
  HARNESS_ORIENTATION_2 = 2

  def __init__(self, ip: str = UP_IP):
    self._up = xmlrpc.client.ServerProxy(f"http://{ip}:{PORT}", allow_none=True)
    self._up.ping()

  def set_harness_orientation(self, orientation: int) -> None:
    self._up.set_harness_orientation(orientation)

  def set_ignition(self, enabled: bool) -> None:
    self._up.set_ignition(enabled)

  def set_can_speed_kbps(self, bus: int, speed: int) -> None:
    self._up.set_can_speed_kbps(bus, speed)

  def can_send(self, addr: int, dat: bytes, bus: int) -> None:
    self._up.can_send(addr, xmlrpc.client.Binary(bytes(dat)), bus)

  def can_send_many(self, msgs) -> None:
    for addr, dat, bus in msgs:
      self.can_send(addr, dat, bus)

  def can_recv(self) -> list[tuple[int, bytes, int]]:
    return [(addr, dat.data, bus) for addr, dat, bus in self._up.can_recv()]

  def can_clear(self, bus: int) -> None:
    self._up.can_clear(bus)

  def close(self) -> None:
    self._up("close")()
