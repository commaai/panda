import random
import unittest

from panda import LEN_TO_DLC, DLC_TO_LEN, pack_can_buffer, unpack_can_buffer
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
  return pkt[0].addr, 0, dat, pkt[0].bus

class PandaCommsTest(unittest.TestCase):
  def test_tx_queues(self):
    for bus in range(4):
      message = (0x100, 0, b"test", bus)

      can_pkt_tx = package_can_msg(message)
      can_pkt_rx = ffi.new('CANPacket_t *')

      assert lpp.can_push(TX_QUEUES[bus], can_pkt_tx), "CAN push failed"
      assert lpp.can_pop(TX_QUEUES[bus], can_pkt_rx), "CAN pop failed"

      assert unpackage_can_msg(can_pkt_rx) == message

  def test_can_send_usb(self):
    for bus in range(3):
      for _ in range(100):
        msgs = []
        for _ in range(200):
          address = random.randint(1, 0x1FFFFFFF)
          data = bytes([random.getrandbits(8) for _ in range(DLC_TO_LEN[random.randrange(0, len(DLC_TO_LEN))])])
          msgs.append((address, 0, data, bus))
        packed = pack_can_buffer(msgs)

        # Simulate USB bulk chunks
        CHUNK_SIZE = 0x40
        for buf in packed:
          for i in range(0, len(buf), CHUNK_SIZE):
            chunk_len = min(CHUNK_SIZE, len(buf) - i)
            lpp.comms_can_write(buf[i:i+chunk_len], chunk_len)

        # Check that they ended up in the right buffers
        queue_msgs = []
        pkt = ffi.new('CANPacket_t *')
        while lpp.can_pop(TX_QUEUES[bus], pkt):
          queue_msgs.append(unpackage_can_msg(pkt))

        assert len(queue_msgs) == len(msgs), f"Expected: {len(msgs)} Queued: {len(queue_msgs)}"
        assert queue_msgs == msgs

  def test_can_receive_usb(self):
    msgs = []
    for _ in range(50000):
      bus = random.randint(0, 3)
      address = random.randint(1, 0x1FFFFFFF)
      data = bytes([random.getrandbits(8) for _ in range(DLC_TO_LEN[random.randrange(0, len(DLC_TO_LEN))])])
      msgs.append((address, 0, data, bus))
    packets = list(map(package_can_msg, msgs))

    rx_msgs = []
    while len(packets) > 0:
      # Push into queue
      while lpp.can_slots_empty(lpp.rx_q) > 0 and len(packets) > 0:
        lpp.can_push(lpp.rx_q, packets.pop(0))

      # Simulate USB bulk IN chunks
      CHUNK_SIZE = 0x40
      MAX_TRANSFER_SIZE = 16384
      dat = ffi.new(f"uint8_t[{CHUNK_SIZE}]")
      while True:
        buf = b""
        while len(buf) < MAX_TRANSFER_SIZE:
          max_size = min(CHUNK_SIZE, MAX_TRANSFER_SIZE - len(buf))
          rx_len = lpp.comms_can_read(dat, max_size)
          buf += bytes(dat[0:rx_len])
          if rx_len < max_size:
            break

        if len(buf) == 0:
          break
        rx_msgs.extend(unpack_can_buffer(buf))

    assert len(rx_msgs) == len(msgs), f"Expected: {len(msgs)} Received: {len(rx_msgs)}"
    assert rx_msgs == msgs

