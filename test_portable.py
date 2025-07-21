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

lpp = libpanda_py.libpanda

def unpackage_can_msg(pkt):
    dat_len = DLC_TO_LEN[pkt[0].data_len_code]
    dat = bytes(pkt[0].data[0:dat_len])
    return pkt[0].addr, dat, pkt[0].bus

class TestPortablePackets(unittest.TestCase):
    def setUp(self):
        lpp.comms_can_reset()

    def test_portable_packet_structure(self):
        """Test that the new portable structure works correctly"""
        
        # Test case
        addr = 0x100
        bus = 1
        data = b"test"
        
        print(f"\n=== Testing portable CAN packet structure ===")
        print(f"Input: addr=0x{addr:x} ({addr}), bus={bus}, data={data}")
        
        # Create packet using make_CANPacket
        can_pkt = libpanda_py.make_CANPacket(addr, bus, data)
        print(f"Created packet type: {type(can_pkt)}")
        
        # Test field access
        print(f"Packet fields:")
        print(f"  addr: {can_pkt[0].addr} (expected: {addr})")
        print(f"  bus: {can_pkt[0].bus} (expected: {bus})")  
        print(f"  data_len_code: {can_pkt[0].data_len_code} (expected: 4)")
        print(f"  extended: {can_pkt[0].extended} (expected: 0)")
        print(f"  checksum: {can_pkt[0].checksum}")
        
        # Test data access
        received_data = bytes([can_pkt[0].data[i] for i in range(len(data))])
        print(f"  data: {received_data} (expected: {data})")
        
        # Test unpackage function
        result = unpackage_can_msg(can_pkt)
        print(f"Unpackaged: {result}")
        expected = (addr, data, bus)
        print(f"Expected: {expected}")
        
        # Assertions
        self.assertEqual(can_pkt[0].addr, addr, "Address mismatch")
        self.assertEqual(can_pkt[0].bus, bus, "Bus mismatch") 
        self.assertEqual(can_pkt[0].data_len_code, 4, "DLC mismatch")
        self.assertEqual(can_pkt[0].extended, 0, "Extended flag mismatch")
        self.assertEqual(received_data, data, "Data mismatch")
        self.assertEqual(result, expected, "Unpackage result mismatch")
        
        print("✅ All assertions passed!")
        
        # Test queue operations
        print(f"\n=== Testing queue operations ===")
        success = lpp.can_push(lpp.tx1_q, can_pkt)
        print(f"Push success: {success}")
        
        can_pkt_rx = libpanda_py.ffi.new('CANPacket_t *')
        pop_success = lpp.can_pop(lpp.tx1_q, can_pkt_rx)
        print(f"Pop success: {pop_success}")
        
        if pop_success:
            # Wrap the popped packet for field access
            wrapped_rx = libpanda_py.CANPacketAccessor(can_pkt_rx)
            print(f"Received fields:")
            print(f"  addr: {wrapped_rx[0].addr}")
            print(f"  bus: {wrapped_rx[0].bus}")
            print(f"  data: {bytes([wrapped_rx[0].data[i] for i in range(len(data))])}")
            
            result_rx = unpackage_can_msg(wrapped_rx)
            print(f"Unpackaged from queue: {result_rx}")
            
            self.assertEqual(result_rx, expected, "Queue roundtrip failed")
            print("✅ Queue operations passed!")

if __name__ == '__main__':
    unittest.main(verbosity=2)