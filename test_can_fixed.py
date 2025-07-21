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

class TestCANFixed(unittest.TestCase):
    def setUp(self):
        lpp.comms_can_reset()

    def test_can_communication(self):
        """Test that CAN communication works with the fixed portable structure"""
        
        print(f"\n=== Testing CAN communication functionality ===")
        
        # Test case from the original failing test
        addr = 256  # 0x100
        bus = 1
        data = b"test"
        
        print(f"Input: addr={addr}, bus={bus}, data={data}")
        
        # Create packet using make_CANPacket
        can_pkt = libpanda_py.make_CANPacket(addr, bus, data)
        print(f"Created packet type: {type(can_pkt)}")
        
        # Verify packet fields
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
        
        # Key assertions that were failing in CI
        self.assertEqual(can_pkt[0].addr, addr, f"Address mismatch: got {can_pkt[0].addr}, expected {addr}")
        self.assertEqual(can_pkt[0].bus, bus, f"Bus mismatch: got {can_pkt[0].bus}, expected {bus}")
        self.assertEqual(can_pkt[0].data_len_code, 4, f"DLC mismatch: got {can_pkt[0].data_len_code}, expected 4")
        self.assertEqual(can_pkt[0].extended, 0, f"Extended flag mismatch: got {can_pkt[0].extended}, expected 0")
        self.assertEqual(received_data, data, f"Data mismatch: got {received_data}, expected {data}")
        self.assertEqual(result, expected, f"Unpackage result mismatch: got {result}, expected {expected}")
        
        print("✅ All packet field assertions passed!")
        
        # Test queue operations
        print(f"\n=== Testing queue operations ===")
        
        # Test queue capacity - this was showing 63 instead of expected higher values
        tx1_capacity = lpp.can_slots_empty(lpp.tx1_q)
        print(f"TX1 queue capacity: {tx1_capacity}")
        
        # Push packet to queue
        success = lpp.can_push(lpp.tx1_q, can_pkt)
        print(f"Push success: {success}")
        self.assertTrue(success, "Failed to push packet to queue")
        
        # Check capacity after push
        after_push_capacity = lpp.can_slots_empty(lpp.tx1_q)
        print(f"TX1 queue capacity after push: {after_push_capacity}")
        
        # Pop packet from queue
        can_pkt_rx = libpanda_py.ffi.new('CANPacket_t *')
        pop_success = lpp.can_pop(lpp.tx1_q, can_pkt_rx)
        print(f"Pop success: {pop_success}")
        self.assertTrue(pop_success, "Failed to pop packet from queue")
        
        if pop_success:
            # Wrap the popped packet for field access
            wrapped_rx = libpanda_py.CANPacketAccessor(can_pkt_rx)
            print(f"Received fields:")
            print(f"  addr: {wrapped_rx[0].addr}")
            print(f"  bus: {wrapped_rx[0].bus}")
            print(f"  data: {bytes([wrapped_rx[0].data[i] for i in range(len(data))])}")
            
            result_rx = unpackage_can_msg(wrapped_rx)
            print(f"Unpackaged from queue: {result_rx}")
            
            self.assertEqual(result_rx, expected, f"Queue roundtrip failed: got {result_rx}, expected {expected}")
            print("✅ Queue operations passed!")
        
        # Check final capacity
        final_capacity = lpp.can_slots_empty(lpp.tx1_q)
        print(f"TX1 queue capacity after pop: {final_capacity}")
        
        # Capacity should be back to original after pop
        self.assertEqual(final_capacity, tx1_capacity, 
                        f"Queue capacity not restored: got {final_capacity}, expected {tx1_capacity}")
        
        print(f"\n✅ All CAN communication tests passed!")

if __name__ == '__main__':
    unittest.main(verbosity=2)