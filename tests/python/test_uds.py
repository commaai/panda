#!/usr/bin/env python3
from parameterized import parameterized_class
import unittest

from panda.python.uds import SERVICE_TYPE, IsoTpMessage, CanClient, get_rx_addr_for_tx_addr


class MockCanBuffer:
  def __init__(self):
    self.rx_msg = None
    self.tx_msgs = []

  def can_send(self, addr, dat, bus):
    self.tx_msgs.append((addr, 0, dat, bus))

  def can_recv(self):
    return [self.rx_msg]


def simulate_isotp_comms(tx_addr: int, rx_addr: int, request: bytes, response: bytes = None,
                         sub_addr: int = None):
  """
  Simulates a client and a server interacting with ISO-TP. openpilot is the client,
  and a car ECU is the simulated server.
  """

  can_buf = MockCanBuffer()
  max_len = 8 if sub_addr is None else 7

  can_client_openpilot = CanClient(can_buf.can_send, can_buf.can_recv, tx_addr, rx_addr, 0, sub_addr)
  can_client_ecu = CanClient(can_buf.can_send, can_buf.can_recv, rx_addr, tx_addr, 0, sub_addr)

  # TODO: handle multiple messages in the buffer and test without single frame mode as well
  isotp_msg_openpilot = IsoTpMessage(can_client_openpilot, timeout=0, max_len=max_len, single_frame_mode=True)
  isotp_msg_ecu = IsoTpMessage(can_client_ecu, timeout=0, max_len=max_len, single_frame_mode=True)

  # setup car ECU
  isotp_msg_ecu.send(b"", setup_only=True)

  # send first message to car ECU and process responses
  isotp_msg_openpilot.send(request)
  can_buf.rx_msg = can_buf.tx_msgs.pop()  # put message to tx in recv buffer

  msg_from_ecu = None
  while not (isotp_msg_openpilot.rx_done and isotp_msg_openpilot.tx_done):
    msg_from_op, _ = isotp_msg_ecu.recv()

    if msg_from_op is None:
      # car ECU is either sending a consecutive frame or a flow control continue to OP
      can_buf.rx_msg = can_buf.tx_msgs.pop()

    else:
      # message complete from openpilot, now respond
      resp_sid = msg_from_op[0] if len(msg_from_op) > 0 else None
      if response is not None:
        isotp_msg_ecu.send(response)
        can_buf.rx_msg = can_buf.tx_msgs.pop()  # update rx message for OP to receive

      elif resp_sid == SERVICE_TYPE.TESTER_PRESENT:
        isotp_msg_ecu.send(bytes([SERVICE_TYPE.TESTER_PRESENT + 0x40]))
        can_buf.rx_msg = can_buf.tx_msgs.pop()  # update rx message for OP to receive

      else:
        raise Exception(f"Service id not supported: {resp_sid}")

    msg_from_ecu, _ = isotp_msg_openpilot.recv()
    if msg_from_ecu is None:
      # openpilot is either sending a consecutive frame or a flow control continue to car ECU
      can_buf.rx_msg = can_buf.tx_msgs.pop()

  return msg_from_ecu


@parameterized_class([
  {"tx_addr": 0x750, "sub_addr": None},
  {"tx_addr": 0x750, "sub_addr": 0xf},
])
class TestUds(unittest.TestCase):
  tx_addr: int
  sub_addr: int

  def setUp(self):
    self.rx_addr = get_rx_addr_for_tx_addr(self.tx_addr)

  def test_tester_present(self):
    """
    Tests IsoTpMessage and CanClient both sending and responding to a
    tester present request with and without sub-addresses
    """

    response = simulate_isotp_comms(self.tx_addr, self.rx_addr, bytes([SERVICE_TYPE.TESTER_PRESENT]), sub_addr=self.sub_addr)
    self.assertEqual(response, bytes([SERVICE_TYPE.TESTER_PRESENT + 0x40]))

  def test_fw_query(self):
    """
    Tests all four ISO-TP frame types in both directions (sending as openpilot and the car ECU)
    """

    for dat_len in range(0x40):
      with self.subTest(dat_len=dat_len):
        # same data for both directions
        data = bytes(range(dat_len))
        response = simulate_isotp_comms(self.tx_addr, self.rx_addr, data, data, self.sub_addr)
        self.assertEqual(response, data)


if __name__ == '__main__':
  unittest.main()
