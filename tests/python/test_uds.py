#!/usr/bin/env python3
import unittest
from unittest.mock import patch
import argparse
from tqdm import tqdm
from panda import Panda
from panda.python.uds import UdsClient, MessageTimeoutError, NegativeResponseError, SESSION_TYPE, DATA_IDENTIFIER_TYPE, CanClient, IsoTpMessage
from functools import partial


# TODO: patch something higher level
class FakePanda(Panda):
  def __init__(self, msg):
    self.msg = msg

  def can_send(self, addr, dat, bus, timeout=0):
    return None

  def can_recv(self):
    return [self.msg]
    # return [self.msgs.pop()]
    # ret.append((address, 0, data, bus))
    # return []


class TestUds(unittest.TestCase):
  def test_fw_rx(self):
    # with patch('panda.python.uds.Panda', FakePanda):
    tx_addr = 0x750
    rx_addr = 0x750 + 0x8
    sub_addr = 0xf
    tx_timeout = 0.1

    panda = FakePanda(msg=[(0x750 + 0x8, 0, b'\x0f\x02~\x00\x00\x00\x00\x00', 0)])

    # uds_client = UdsClient(fake_panda, (tx_addr, sub_addr), 0x750 + 8, 0, timeout=0.2, debug=True)
    can_send_with_timeout = partial(panda.can_send, timeout=int(tx_timeout * 1000))
    partial(panda.can_send, timeout=int(tx_timeout * 1000))
    can_client = CanClient(can_send_with_timeout, panda.can_recv, tx_addr, rx_addr, 0, sub_addr, debug=True)
    isotp_msg = IsoTpMessage(can_client, timeout=0, max_len=7)
    isotp_msg.send(b'\x3e')

    dat, updated = isotp_msg.recv(0)
    self.assertEqual(dat, b'\x7e\x00')
    #
    # can_client = CanClient(can_send_with_timeout, panda.can_recv, self.tx_addr, self.rx_addr, self.bus, debug=self.debug)
    #
    # can_client = CanClient(self._can_tx, partial(self._can_rx, rx_addr, sub_addr=sub_addr), tx_addr, rx_addr,
    #                        self.bus, sub_addr=sub_addr, debug=self.debug)


if __name__ == '__main__':
  unittest.main()
