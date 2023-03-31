#!/usr/bin/env python3
import unittest
from unittest.mock import patch
from panda import Panda
from panda.python.uds import UdsClient, MessageTimeoutError, NegativeResponseError, SERVICE_TYPE, SESSION_TYPE, DATA_IDENTIFIER_TYPE, CanClient, IsoTpMessage
from functools import partial
from parameterized import parameterized


# TODO: patch something higher level
class FakePanda(Panda):
  def __init__(self, msg):
    self.msg = msg

  def can_send(self, addr, dat, bus, timeout=0):
    return None

  def can_recv(self):
    return [self.msg]


class TestUds(unittest.TestCase):
  @parameterized.expand([
    (0x750, 0xf, 0x750 + 8),
    (0x750, None, 0x750 + 8),
  ])
  def test_fw_query(self, tx_addr, sub_addr, rx_addr):
    tx_timeout = 0.1

    ecu_rx_dat = [0x02, SERVICE_TYPE.TESTER_PRESENT + 0x40, 0x0]
    if sub_addr is not None:
      ecu_rx_dat.insert(0, sub_addr)
    ecu_rx_dat.extend([0x0] * (8 - len(ecu_rx_dat)))
    ecu_rx_dat = bytes(ecu_rx_dat)

    panda = FakePanda(msg=(0x750 + 0x8, 0, bytes(ecu_rx_dat), 0))

    partial(panda.can_send, timeout=int(tx_timeout * 1000))
    can_client = CanClient(panda.can_send, panda.can_recv, tx_addr, rx_addr, 0, sub_addr, debug=True)
    isotp_msg = IsoTpMessage(can_client, timeout=0, max_len=7)
    isotp_msg.send(b'\x3e')

    dat, updated = isotp_msg.recv(0)
    self.assertEqual(dat, b'\x7e\x00')


if __name__ == '__main__':
  unittest.main()
