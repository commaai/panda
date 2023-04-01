#!/usr/bin/env python3
import struct
import time
import unittest
from unittest.mock import patch
from panda import Panda
from panda.python.uds import UdsClient, MessageTimeoutError, NegativeResponseError, SERVICE_TYPE, SESSION_TYPE, DATA_IDENTIFIER_TYPE, IsoTpMessage, CanClient, get_rx_addr_for_tx_addr
from parameterized import parameterized
from parameterized import parameterized_class
import itertools


def p16(val):
  return struct.pack("!H", val)


class FakePanda(Panda):
  def __init__(self, msg):
    self.msg = msg
    # self.last_tx_msg = None
    self.tx_msgs = []

  def can_send(self, addr, dat, bus, timeout=0):
    # self.last_tx_msg = (addr, 0, dat, bus)
    self.tx_msgs.append((addr, 0, dat, bus))

  def can_recv(self):
    return [self.msg]


def make_response_dat(service_type, sub_addr, data: bytes = None):
  ecu_rx_dat = [service_type + 0x40]
  if data is not None:
    ecu_rx_dat += data
  ecu_rx_dat.insert(0, len(ecu_rx_dat))
  if sub_addr is not None:
    ecu_rx_dat.insert(0, sub_addr)
  ecu_rx_dat.extend([0x0] * (8 - len(ecu_rx_dat)))
  return bytes(ecu_rx_dat)


def simulate_isotp_comms(tx_addr: int, rx_addr: int, request: bytes, response: bytes = None,
                         sub_addr: int = None):
  panda = FakePanda([])
  max_len = 8 if sub_addr is None else 7

  can_client_openpilot = CanClient(panda.can_send, panda.can_recv, tx_addr, rx_addr, 0, sub_addr, debug=True)
  can_client_ecu = CanClient(panda.can_send, panda.can_recv, rx_addr, tx_addr, 0, sub_addr, debug=True)

  # TODO: handle multiple messages in the buffer and test without single frame mode as well
  isotp_msg_openpilot = IsoTpMessage(can_client_openpilot, timeout=0, debug=True, max_len=max_len, single_frame_mode=True)
  isotp_msg_ecu = IsoTpMessage(can_client_ecu, timeout=0, debug=True, max_len=max_len, single_frame_mode=True)

  # setup car ECU
  isotp_msg_ecu.send(b"", setup_only=True)

  # send first message to car ECU and process responses
  isotp_msg_openpilot.send(request)
  panda.msg = panda.tx_msgs.pop()  # put message to tx in recv buffer

  while not (isotp_msg_openpilot.rx_done and isotp_msg_openpilot.rx_done):
    # car ECU receives OP's message
    msg_from_op, _ = isotp_msg_ecu.recv()

    if msg_from_op is None:
      # Car ECU is either sending a consecutive frame or a flow control continue to OP
      panda.msg = panda.tx_msgs.pop()

    else:
      # Message complete from openpilot, now respond
      resp_sid = msg_from_op[0] if len(msg_from_op) > 0 else None
      if response is not None:
        isotp_msg_ecu.send(response)
        panda.msg = panda.tx_msgs.pop()  # update rx message for OP to receive

      elif resp_sid == SERVICE_TYPE.TESTER_PRESENT:
        isotp_msg_ecu.send(bytes([SERVICE_TYPE.TESTER_PRESENT + 0x40]))
        panda.msg = panda.tx_msgs.pop()  # update rx message for OP to receive

      else:
        raise Exception(f"Service id not supported: {resp_sid}")

    msg, _ = isotp_msg_openpilot.recv()

    # Message complete from car ECU
    if msg is not None:
      return msg

    # openpilot is either sending a consecutive frame or a flow control continue to car ECU
    panda.msg = panda.tx_msgs.pop()


@parameterized_class([
  {"tx_addr": 0x750, "sub_addr": None},
  {"tx_addr": 0x750, "sub_addr": 0xf},
])
class TestUds(unittest.TestCase):
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
