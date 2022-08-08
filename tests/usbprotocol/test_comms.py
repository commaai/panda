import unittest

from panda import LEN_TO_DLC, DLC_TO_LEN
from panda.tests.usbprotocol.libpandaprotocol_py import ffi, libpandaprotocol as lpp

TX_QUEUES = (lpp.tx1_q, lpp.tx2_q, lpp.tx3_q, lpp.txgmlan_q)

def package_can_msg(msg):
  addr, _, dat, bus = msg
  ret = ffi.new('CANPacket_t *')
  ret[0].extended = 1 if addr >= 0x800 else 0
  ret[0].addr = addr
  ret[0].data_len_code = LEN_TO_DLC[len(dat)]
  ret[0].bus = bus
  ret[0].data = bytes(dat)
  return ret

def unpackage_can_msg(pkt):
  dat_len = DLC_TO_LEN[pkt[0].data_len_code]
  dat = bytes(pkt[0].data[0:dat_len])
  return pkt[0].addr, None, dat, pkt[0].bus

class PandaCommsTest(unittest.TestCase):
  def test_tx_queues(self):
    for bus in range(4):
      message = (0x100, None, b"test", bus)

      can_pkt_tx = package_can_msg(message)
      can_pkt_rx = ffi.new('CANPacket_t *')

      assert lpp.can_push(TX_QUEUES[bus], can_pkt_tx), "CAN push failed"
      assert lpp.can_pop(TX_QUEUES[bus], can_pkt_rx), "CAN pop failed"

      assert unpackage_can_msg(can_pkt_rx) == message


