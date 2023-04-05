#!/usr/bin/env python3
from parameterized import parameterized_class
import unittest
from unittest.mock import patch
import threading

from panda.python.uds import SERVICE_TYPE, DATA_IDENTIFIER_TYPE, DEFAULT_VIN, IsoTpMessage, CanClient, UdsClient, get_rx_addr_for_tx_addr


# @patch('panda.python.uds.CanClient', )
class UdsServer(UdsClient):
  pass


class MockCanBuffer:
  def __init__(self):
    self.lock = threading.Lock()
    self.rx_msg = None
    self.tx_msgs = []

  def can_send(self, addr, dat, bus, timeout=0):
    with self.lock:
      # print('can client here, adding to tx_msgs')
      self.tx_msgs.append((addr, 0, dat, bus))
      self.rx_msg = self.tx_msgs.pop()

  def can_recv(self):
    with self.lock:
      # print('can client here, returning', [self.rx_msg] if self.rx_msg else [])
      return [self.rx_msg] if self.rx_msg else []


@parameterized_class([
  # {"tx_addr": 0x750, "sub_addr": None},
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

    kill_event = threading.Event()
    can_buf = MockCanBuffer()

    uds_server = UdsClient(can_buf, self.rx_addr, self.tx_addr, sub_addr=self.sub_addr, timeout=0)
    uds_client = UdsClient(can_buf, self.tx_addr, self.rx_addr, sub_addr=self.sub_addr)

    uds_thread = threading.Thread(target=uds_server._uds_response, args=(kill_event,))

    try:
      uds_thread.start()
      uds_client.tester_present()
      response = uds_client.read_data_by_identifier(DATA_IDENTIFIER_TYPE.VIN)
      self.assertEqual(response.decode('utf-8'), DEFAULT_VIN)
      print('Client: got resp:', response)
    finally:
      kill_event.set()
      uds_thread.join()

  # def test_fw_query(self):
  #   """
  #   Tests all four ISO-TP frame types in both directions (sending as openpilot and the car ECU)
  #   """
  #
  #   for dat_len in range(0x40):
  #     with self.subTest(dat_len=dat_len):
  #       # same data for both directions
  #       data = bytes(range(dat_len))
  #       response = simulate_isotp_comms(self.tx_addr, self.rx_addr, data, data, self.sub_addr)
  #       self.assertEqual(response, data)


if __name__ == '__main__':
  unittest.main()
