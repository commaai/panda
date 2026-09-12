import ctypes
import unittest

from panda import pack_can_buffer
from panda.tests.libpanda import libpanda_py as lp


def byte_buffer(data):
  if isinstance(data, int):
    return (ctypes.c_uint8 * data)()
  # Keep the trailing sentinel used by the alignment/bounds assertions.
  return (ctypes.c_uint8 * (len(data) + 1))(*data)


def buffer_pointer(buffer, offset):
  return ctypes.cast(ctypes.byref(buffer, offset), ctypes.POINTER(ctypes.c_uint8))


class TestSpiAdmission(unittest.TestCase):
  def setUp(self):
    self.p = lp.libpanda
    self.process_can_calls = (ctypes.c_uint32 * 3).in_dll(self.p, "process_can_calls")
    self.process_can_batch_calls = (ctypes.c_uint32 * 3).in_dll(self.p, "process_can_batch_calls")
    self.process_can_batch_budget = (ctypes.c_uint32 * 3).in_dll(self.p, "process_can_batch_budget")
    self.safety_tx_blocked = ctypes.c_uint32.in_dll(self.p, "safety_tx_blocked")
    self.queues = [self.p.tx1_q, self.p.tx2_q, self.p.tx3_q, self.p.rx_q]
    self.p.comms_can_reset()
    for queue in self.queues:
      self.p.can_clear(queue)
    self.p.set_safety_hooks(17, 0)  # allOutput
    for bus in range(3):
      self.process_can_calls[bus] = 0
      self.process_can_batch_calls[bus] = 0
      self.process_can_batch_budget[bus] = 0
    self.filler = lp.make_CANPacket(0x123, 0, b"abcdefgh")

  def tearDown(self):
    self.p.comms_can_reset()
    for queue in self.queues:
      self.p.can_clear(queue)

  @staticmethod
  def wire(messages):
    return b"".join(pack_can_buffer(messages, chunk=True))

  def write(self, data):
    return self.p.comms_can_write_checked(data, len(data))

  def snapshot(self):
    return ([(queue.contents.w_ptr, queue.contents.r_ptr) for queue in self.queues],
            list(self.process_can_calls), list(self.process_can_batch_calls), self.safety_tx_blocked.value)

  def fill(self, queue):
    while self.p.can_slots_empty(queue):
      self.assertTrue(self.p.can_push(queue, self.filler))

  def test_partial_record_survives_rejection(self):
    data = self.wire([(0x123, b"abcdefgh", 0)])
    self.assertTrue(self.write(data[:3]))
    self.fill(self.p.tx1_q)
    before = self.snapshot()
    self.assertFalse(self.write(data[3:]))
    self.assertEqual(self.snapshot(), before)
    self.p.can_clear(self.p.tx1_q)
    self.assertTrue(self.write(data[3:]))
    packet = lp.CANPacket()
    self.assertTrue(self.p.can_pop(self.p.tx1_q, packet))
    self.assertEqual(bytes(packet.data[0:8]), b"abcdefgh")
    self.assertFalse(self.p.can_pop(self.p.tx1_q, packet))

  def test_mixed_bus_rejection_has_no_effects(self):
    self.fill(self.p.tx2_q)
    before = self.snapshot()
    self.assertFalse(self.write(self.wire([(0x123, b"abcdefgh", 0), (0x124, b"abcdefgh", 1)])))
    self.assertEqual(self.snapshot(), before)

  def test_receipt_capacity_rejection_has_no_effects(self):
    self.fill(self.p.rx_q)
    before = self.snapshot()
    self.assertFalse(self.write(self.wire([(0x123, b"abcdefgh", 0)])))
    self.assertEqual(self.snapshot(), before)

  def test_admits_multiple_buses(self):
    self.assertTrue(self.write(self.wire([(0x123, b"", bus) for bus in range(3)])))
    packet = lp.CANPacket()
    for queue in self.queues[:3]:
      self.assertTrue(self.p.can_pop(queue, packet))

  def test_batch_kicks_once_per_bus_and_preserves_order(self):
    messages = [(0x120 + i, bytes([i]) * 8, i % 3) for i in range(120)]
    self.assertTrue(self.write(self.wire(messages)))
    self.assertEqual(list(self.process_can_calls), [0, 0, 0])
    self.assertEqual(list(self.process_can_batch_calls), [1, 1, 1])
    self.assertEqual(list(self.process_can_batch_budget), [40, 40, 40])
    packet = lp.CANPacket()
    for bus, queue in enumerate(self.queues[:3]):
      for addr, data, msg_bus in messages:
        if msg_bus == bus:
          self.assertTrue(self.p.can_pop(queue, packet))
          self.assertEqual(packet.addr, addr)
          self.assertEqual(bytes(packet.data[0:8]), data)
      self.assertFalse(self.p.can_pop(queue, packet))

  def test_legacy_usb_still_kicks_each_record(self):
    data = self.wire([(0x123, b"abcdefgh", 1)] * 10)
    self.p.comms_can_write(data, len(data))
    self.assertEqual(list(self.process_can_calls), [0, 10, 0])
    self.assertEqual(list(self.process_can_batch_calls), [0, 0, 0])

  def test_safety_rejections_keep_order_without_kicks(self):
    self.p.set_safety_hooks(0, 0)  # silent
    messages = [(0x120 + i, bytes([i]), i % 3) for i in range(12)]
    blocked = self.safety_tx_blocked.value
    self.assertTrue(self.write(self.wire(messages)))
    self.assertEqual(self.safety_tx_blocked.value - blocked, len(messages))
    self.assertEqual(list(self.process_can_calls), [0, 0, 0])
    self.assertEqual(list(self.process_can_batch_calls), [0, 0, 0])
    packet = lp.CANPacket()
    for addr, data, bus in messages:
      self.assertTrue(self.p.can_pop(self.p.rx_q, packet))
      self.assertEqual((packet.addr, packet.bus, packet.rejected, packet.returned), (addr, bus, 1, 0))
      self.assertEqual(bytes(packet.data[0:1]), data)
      raw = bytes(packet)[:7]
      checksum = 0
      for byte in raw:
        checksum ^= byte
      self.assertEqual(checksum, 0)
    self.assertFalse(self.p.can_pop(self.p.rx_q, packet))

  def test_partial_record_does_not_kick_until_complete(self):
    data = self.wire([(0x123, b"abcdefgh", 2)])
    self.assertTrue(self.write(data[:3]))
    self.assertEqual(list(self.process_can_batch_calls), [0, 0, 0])
    self.assertTrue(self.write(data[3:]))
    self.assertEqual(list(self.process_can_batch_calls), [0, 0, 1])

  def test_forwarding_still_bypasses_hook_and_kicks_immediately(self):
    self.p.set_safety_hooks(0, 0)
    blocked = self.safety_tx_blocked.value
    self.p.can_send(self.filler, 2, True)
    self.assertEqual(self.safety_tx_blocked.value, blocked)
    self.assertEqual(list(self.process_can_calls), [0, 0, 1])
    self.assertEqual(list(self.process_can_batch_calls), [0, 0, 0])
    packet = lp.CANPacket()
    self.assertTrue(self.p.can_pop(self.p.tx3_q, packet))
    self.assertEqual(packet.addr, self.filler.addr)

  def test_all_dlc_records_at_every_split(self):
    for length in (0, 1, 2, 3, 4, 5, 6, 7, 8, 12, 16, 20, 24, 32, 48, 64):
      payload = bytes(range(length))
      data = self.wire([(0x123, payload, 1), (0x124, b"tail", 2)])
      for split in range(1, len(data)):
        with self.subTest(length=length, split=split):
          self.p.comms_can_reset()
          self.assertTrue(self.write(data[:split]))
          self.assertTrue(self.write(b""))
          self.assertTrue(self.write(data[split:]))
          packet = lp.CANPacket()
          for queue, addr, expected in ((self.p.tx2_q, 0x123, payload), (self.p.tx3_q, 0x124, b"tail")):
            self.assertTrue(self.p.can_pop(queue, packet))
            self.assertEqual(packet.addr, addr)
            self.assertEqual(bytes(packet.data[0:len(expected)]), expected)
            self.assertFalse(self.p.can_pop(queue, packet))

  def test_invalid_partial_header_rejects_entire_write(self):
    valid = self.wire([(0x123, b"abcdefgh", 0)])
    for bus in range(3, 8):
      before = self.snapshot()
      self.assertFalse(self.write(valid + bytes([bus << 1])))
      self.assertEqual(self.snapshot(), before)
    self.assertTrue(self.write(valid))
    packet = lp.CANPacket()
    self.assertTrue(self.p.can_pop(self.p.tx1_q, packet))
    self.assertEqual(packet.addr, 0x123)
    self.assertFalse(self.p.can_pop(self.p.tx1_q, packet))

  def test_wire_copy_all_lengths_and_alignments(self):
    payload = bytes(range(70))
    for batch in (False, True):
      for length in range(71):
        for src_offset in range(4):
          for dst_offset in range(4):
            with self.subTest(batch=batch, length=length, src_offset=src_offset, dst_offset=dst_offset):
              source = byte_buffer(b"S" * src_offset + payload[:length] + b"S" * 4)
              dest = byte_buffer(b"D" * 80)
              before = bytes(source)
              self.p.comms_can_wire_copy(buffer_pointer(dest, dst_offset), buffer_pointer(source, src_offset), length, batch)
              expected = b"D" * dst_offset + payload[:length] + b"D" * (80 - dst_offset - length) + b"\x00"
              self.assertEqual(bytes(dest), expected)
              self.assertEqual(bytes(source), before)

  def test_misaligned_fd_partial_stream(self):
    payload = bytes(range(64))
    data = self.wire([(0x123, payload, 0), (0x124, payload[::-1], 0)])
    for offset in range(4):
      for split in range(1, len(data)):
        with self.subTest(offset=offset, split=split):
          self.p.comms_can_reset()
          for chunk in (data[:split], data[split:]):
            source = byte_buffer(b"X" * offset + chunk)
            self.assertTrue(self.p.comms_can_write_checked(buffer_pointer(source, offset), len(chunk)))
          packet = lp.CANPacket()
          for addr, expected in ((0x123, payload), (0x124, payload[::-1])):
            self.assertTrue(self.p.can_pop(self.p.tx1_q, packet))
            self.assertEqual(packet.addr, addr)
            self.assertEqual(bytes(packet.data[0:64]), expected)
          self.assertFalse(self.p.can_pop(self.p.tx1_q, packet))

  def test_spi_read_all_dlc_and_chunk_boundaries(self):
    messages = [(0x123 + i, bytes(range(length)), i % 3)
                for i, length in enumerate((0, 1, 2, 3, 4, 5, 6, 7, 8, 12, 16, 20, 24, 32, 48, 64))]
    expected = self.wire(messages)
    for offset in range(4):
      for capacity in range(1, 72):
        with self.subTest(offset=offset, capacity=capacity):
          self.p.comms_can_reset()
          for addr, data, bus in messages:
            self.assertTrue(self.p.can_push(self.p.rx_q, lp.make_CANPacket(addr, bus, data)))
          output = bytearray()
          while len(output) < len(expected):
            dest = byte_buffer(b"D" * (capacity + 8))
            self.assertEqual(self.p.comms_can_read_spi(buffer_pointer(dest, offset), 0), 0)
            count = self.p.comms_can_read_spi(buffer_pointer(dest, offset), capacity)
            self.assertGreater(count, 0)
            self.assertLessEqual(count, capacity)
            raw = bytes(dest)
            self.assertEqual(raw[:offset], b"D" * offset)
            self.assertEqual(raw[offset + count:-1], b"D" * (capacity + 8 - offset - count))
            output.extend(raw[offset:offset + count])
          self.assertEqual(output, expected)
          self.assertEqual(self.p.comms_can_read_spi(buffer_pointer(dest, offset), capacity), 0)

  def test_spi_and_usb_share_partial_read_record(self):
    messages = [(0x123, bytes(range(64)), 0), (0x124, b"tail", 1)]
    for addr, data, bus in messages:
      self.assertTrue(self.p.can_push(self.p.rx_q, lp.make_CANPacket(addr, bus, data)))
    output = bytearray()
    for reader, capacity in ((self.p.comms_can_read_spi, 3), (self.p.comms_can_read, 11),
                             (self.p.comms_can_read_spi, 60), (self.p.comms_can_read, 70)):
      dest = byte_buffer(capacity)
      count = reader(dest, capacity)
      output.extend(bytes(dest[:count]))
    self.assertEqual(output, self.wire(messages))

  def test_spi_read_fills_capacity_until_stream_empty(self):
    messages = [(0x100 + i, b"", i % 3) for i in range(830)]
    expected = self.wire(messages)
    for addr, data, bus in messages:
      self.assertTrue(self.p.can_push(self.p.rx_q, lp.make_CANPacket(addr, bus, data)))
    dest = byte_buffer(4096)
    output = bytearray()
    lengths = []
    while True:
      count = self.p.comms_can_read_spi(dest, 4096)
      if count == 0:
        break
      lengths.append(count)
      output.extend(bytes(dest[:count]))
    self.assertEqual(lengths, [4096, 884])
    self.assertEqual(output, expected)

  def test_spi_read_finishes_carry_and_fills_remaining_capacity(self):
    messages = [(0x100 + i, b"", i % 3) for i in range(830)]
    expected = self.wire(messages)
    for prefix_capacity in (3, 255 * 6 + 3):
      with self.subTest(prefix_capacity=prefix_capacity):
        self.p.comms_can_reset()
        for addr, data, bus in messages:
          self.assertTrue(self.p.can_push(self.p.rx_q, lp.make_CANPacket(addr, bus, data)))
        dest = byte_buffer(4096)
        count = self.p.comms_can_read_spi(dest, prefix_capacity)
        self.assertEqual(count, prefix_capacity)
        output = bytearray(bytes(dest[:count]))
        count = self.p.comms_can_read_spi(dest, 4096)
        self.assertEqual(count, min(4096, len(expected) - prefix_capacity))
        output.extend(bytes(dest[:count]))
        while True:
          count = self.p.comms_can_read_spi(dest, 4096)
          if count == 0:
            break
          output.extend(bytes(dest[:count]))
        self.assertEqual(output, expected)

  def test_legacy_usb_read_has_no_record_cap(self):
    messages = [(0x100 + i, b"", i % 3) for i in range(830)]
    for addr, data, bus in messages:
      self.assertTrue(self.p.can_push(self.p.rx_q, lp.make_CANPacket(addr, bus, data)))
    dest = byte_buffer(4096)
    count = self.p.comms_can_read(dest, 4096)
    self.assertEqual(count, 4096)
    output = bytearray(bytes(dest[:count]))
    count = self.p.comms_can_read(dest, 4096)
    output.extend(bytes(dest[:count]))
    self.assertEqual(output, self.wire(messages))

  def test_spi_direct_read_wraps_queue_without_losing_mixed_records(self):
    queue = self.p.rx_q
    # Exercise the last two physical slots and then index zero.
    queue.contents.r_ptr = queue.contents.fifo_size - 2
    queue.contents.w_ptr = queue.contents.r_ptr
    lengths = (64, 0, 8, 12, 1, 48, 3, 20)
    messages = [(0x123 + i, bytes([i]) * length, i % 3) for i, length in enumerate(lengths)]
    for addr, data, bus in messages:
      self.assertTrue(self.p.can_push(queue, lp.make_CANPacket(addr, bus, data)))
    write_pointer = queue.contents.w_ptr
    expected = self.wire(messages)
    output = bytearray()
    dest = byte_buffer(4097)
    # First read holds most of a 64-byte record in carry; second crosses wrap.
    for capacity in (3, 71, 4096):
      before = self.p.can_slots_empty(queue)
      count = self.p.comms_can_read_spi(buffer_pointer(dest, 1), capacity)
      output.extend(bytes(dest[1:1 + count]))
      self.assertGreaterEqual(self.p.can_slots_empty(queue), before)
      self.assertEqual(queue.contents.w_ptr, write_pointer)
    self.assertEqual(output, expected)
    self.assertEqual(queue.contents.r_ptr, queue.contents.w_ptr)
    self.assertEqual(self.p.can_slots_empty(queue), queue.contents.fifo_size - 1)
    self.assertEqual(self.p.comms_can_read_spi(dest, 4096), 0)

  def test_spi_carry_survives_source_slot_reuse(self):
    queue = self.p.rx_q
    payload = bytes(range(64))
    packet = lp.make_CANPacket(0x123, 0, payload)
    self.assertTrue(self.p.can_push(queue, packet))
    original_slot = queue.contents.r_ptr
    dest = byte_buffer(80)
    count = self.p.comms_can_read_spi(dest, 1)
    output = bytearray(bytes(dest[:count]))
    self.assertEqual(queue.contents.r_ptr, queue.contents.w_ptr)
    # After release, the producer may reuse the source slot. Carry must own its bytes.
    ctypes.memmove(ctypes.addressof(queue.contents.elems[original_slot]), b"Z" * 70, 70)
    self.assertEqual(self.p.comms_can_read_spi(dest, 0), 0)
    count = self.p.comms_can_read_spi(dest, 80)
    output.extend(bytes(dest[:count]))
    self.assertEqual(output, self.wire([(0x123, payload, 0)]))
    self.assertEqual(self.p.comms_can_read_spi(dest, 80), 0)
