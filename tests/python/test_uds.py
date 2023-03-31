#!/usr/bin/env python3
import unittest
from unittest.mock import patch
from panda import Panda
from panda.python.uds import UdsClient, MessageTimeoutError, NegativeResponseError, SERVICE_TYPE, SESSION_TYPE, DATA_IDENTIFIER_TYPE
from parameterized import parameterized


class FakePanda(Panda):
  def __init__(self, msg):
    self.msg = msg

  def can_send(self, addr, dat, bus, timeout=0):
    return None

  def can_recv(self):
    return [self.msg]


def make_response_dat(sub_addr):
  ecu_rx_dat = [0x02, SERVICE_TYPE.TESTER_PRESENT + 0x40, 0x0]
  if sub_addr is not None:
    ecu_rx_dat.insert(0, sub_addr)
  ecu_rx_dat.extend([0x0] * (8 - len(ecu_rx_dat)))
  return bytes(ecu_rx_dat)


class TestUds(unittest.TestCase):
  @parameterized.expand([
    (0x750, 0xf, 0x750 + 8),
    (0x750, None, 0x750 + 8),
  ])
  def test_uds_client_tester_present(self, tx_addr, sub_addr, rx_addr):
    ecu_response_dat = make_response_dat(sub_addr)
    panda = FakePanda(msg=(0x750 + 0x8, 0, bytes(ecu_response_dat), 0))
    uds_client = UdsClient(panda, tx_addr, rx_addr, sub_addr=sub_addr)
    uds_client.tester_present()


if __name__ == '__main__':
  unittest.main()
