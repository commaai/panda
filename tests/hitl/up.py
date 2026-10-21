#!/usr/bin/env python3
"""The HITL tests' stand-in for a panda jungle: an ultimate provisioning (UP) board.

The tests run on the DUT, and the DUT's internal panda is wired to the UP's OBD-C port.
`up.py serve` runs on the UP and exposes its harness and CAN over the USB network
between the UP and the DUT, and UPJungle is the jungle-like client the tests use.
"""
import os
import subprocess
import sys
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


def serve() -> None:
  from xmlrpc.server import SimpleXMLRPCServer
  from ultimate_provisioning import UP, CAN, available_interfaces, configure

  up = UP()
  interfaces = available_interfaces()
  bitrates = dict.fromkeys(range(len(interfaces)), 500_000)
  configure(interfaces, bitrate=500_000)
  can = CAN(interfaces)
  state = {"orientation": UPJungle.HARNESS_ORIENTATION_NONE, "ignition": False}

  def apply_harness():
    # with the harness in orientation 1, UP canN is panda bus N
    if state["orientation"] == UPJungle.HARNESS_ORIENTATION_1:
      up.orientation_sbu2(ignition_on=state["ignition"])
    elif state["orientation"] == UPJungle.HARNESS_ORIENTATION_2:
      up.orientation_sbu1(ignition_on=state["ignition"])
    else:
      up.reset_obdc_gpio()

  def set_harness_orientation(orientation):
    state["orientation"] = orientation
    apply_harness()

  def set_ignition(enabled):
    state["ignition"] = enabled
    apply_harness()

  def restart(bus):
    # takes the interface down and up, which also drops frames stuck in its TX FIFO, e.g. when nothing ACKs
    i = interfaces[bus]
    subprocess.run(f"sudo ip link set {i} down && sudo ip link set {i} type can bitrate {bitrates[bus]} restart-ms 100 && "
                   f"sudo ip link set {i} up", shell=True, check=True)

  def set_can_speed_kbps(bus, speed):
    if bitrates[bus] != speed * 1000:
      bitrates[bus] = speed * 1000
      restart(bus)

  def can_send(addr, dat, bus):
    can.can_send(addr, dat.data, bus)

  def can_recv():
    return [(addr, xmlrpc.client.Binary(dat), bus) for addr, dat, bus in can.can_recv()]

  def can_clear(bus):
    if bus == 0xFFFF:
      can.can_recv()
    elif bus in bitrates:
      restart(bus)

  def reset():
    for bus in bitrates:
      bitrates[bus] = 500_000
      restart(bus)
    can.can_recv()
    set_harness_orientation(UPJungle.HARNESS_ORIENTATION_NONE)

  server = SimpleXMLRPCServer((UP_IP, PORT), allow_none=True, logRequests=False)
  for f in (set_harness_orientation, set_ignition, set_can_speed_kbps, can_send, can_recv, can_clear, reset):
    server.register_function(f)
  server.register_function(lambda: True, "ping")
  reset()
  print(f"serving on {UP_IP}:{PORT}", flush=True)
  try:
    server.serve_forever()
  finally:
    up.reset_obdc_gpio()


if __name__ == "__main__":
  if sys.argv[1:] == ["serve"]:
    serve()
  else:
    print(f"usage: {sys.argv[0]} serve")
    sys.exit(1)
