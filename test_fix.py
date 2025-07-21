#!/usr/bin/env python3
import sys
import unittest

# Add paths for imports
sys.path.insert(0, '.')
sys.path.insert(0, 'python')

# Mock the missing imports
class MockCarParams:
    class SafetyModel:
        allOutput = 0x1337

# Set up mock modules
sys.modules['opendbc'] = type(sys)('opendbc')
sys.modules['opendbc.car'] = type(sys)('opendbc.car')
sys.modules['opendbc.car.structs'] = type(sys)('opendbc.car.structs')
sys.modules['opendbc.car.structs'].CarParams = MockCarParams

from tests.libpanda import libpanda_py

# Constants
DLC_TO_LEN = [0, 1, 2, 3, 4, 5, 6, 7, 8, 12, 16, 20, 24, 32, 48, 64]
USBPACKET_MAX_SIZE = 0x40

lpp = libpanda_py.libpanda
TX_QUEUES = (lpp.tx1_q, lpp.tx2_q, lpp.tx3_q)

def unpackage_can_msg(pkt):
    dat_len = DLC_TO_LEN[pkt[0].data_len_code]
    dat = bytes(pkt[0].data[0:dat_len])
    return pkt[0].addr, dat, pkt[0].bus

class TestBasicFunctionality(unittest.TestCase):
    def setUp(self):
        lpp.comms_can_reset()

    def test_tx_queues(self):
        """Test that basic queue operations work correctly"""
        for bus in range(len(TX_QUEUES)):
            message = (0x100, b"test", bus)

            can_pkt_tx = libpanda_py.make_CANPacket(message[0], message[2], message[1])
            can_pkt_rx = libpanda_py.ffi.new('CANPacket_t *')

            # Debug info
            print(f"\nBus {bus}:")
            print(f"Original message: {message}")
            print(f"Created packet - addr: {can_pkt_tx[0].addr}, bus: {can_pkt_tx[0].bus}, data: {[can_pkt_tx[0].data[i] for i in range(4)]}")

            self.assertTrue(lpp.can_push(TX_QUEUES[bus], can_pkt_tx), "CAN push failed")
            self.assertTrue(lpp.can_pop(TX_QUEUES[bus], can_pkt_rx), "CAN pop failed")

            print(f"Received packet - addr: {can_pkt_rx[0].addr}, bus: {can_pkt_rx[0].bus}, data: {[can_pkt_rx[0].data[i] for i in range(4)]}")

            result = unpackage_can_msg(can_pkt_rx)
            print(f"Unpackaged result: {result}")

            self.assertEqual(result, message, f"Message mismatch for bus {bus}")

if __name__ == '__main__':
    unittest.main()