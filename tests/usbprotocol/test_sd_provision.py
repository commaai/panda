#!/usr/bin/env python3
import importlib.util
import pathlib
import struct
import tempfile
import unittest

# provision_sd.py is a standalone script, not part of a package — load by path.
_script = pathlib.Path(__file__).parents[2] / "board/jungle/scripts/provision_sd.py"
_spec = importlib.util.spec_from_file_location("provision_sd", _script)
_mod = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(_mod)

build_header  = _mod.build_header
build_records = _mod.build_records
write_image   = _mod.write_image
HEADER_FMT    = _mod.HEADER_FMT
MAGIC         = _mod.MAGIC
FORMAT_VERSION = _mod.FORMAT_VERSION
RECORD_FMT    = _mod.RECORD_FMT
RECORD_SIZE   = _mod.RECORD_SIZE
SECTOR_SIZE   = _mod.SECTOR_SIZE

# nanoseconds per microsecond
_US = 1000


def _make_messages(n, t0_ns=0, step_ns=1_000_000):
  """Generate n synthetic (mono_time_ns, addr, bus, data) messages."""
  return [(t0_ns + i * step_ns, 0x100 + i, i % 3, bytes([i % 256] * 8)) for i in range(n)]


def _unpack_record(raw):
  """Return (mono_time_us, addr, bus, data_len, data, pad) from 20-byte record."""
  return struct.unpack(RECORD_FMT, raw)


class TestSDHeader(unittest.TestCase):
  def test_magic(self):
    header = build_header(10)
    self.assertEqual(header[:8], MAGIC)

  def test_record_count(self):
    for n in (0, 1, 100, 65535):
      header = build_header(n)
      _, num_records, _, _ = struct.unpack(HEADER_FMT, header[:struct.calcsize(HEADER_FMT)])
      self.assertEqual(num_records, n)

  def test_record_size_field(self):
    header = build_header(1)
    _, _, record_size, _ = struct.unpack(HEADER_FMT, header[:struct.calcsize(HEADER_FMT)])
    self.assertEqual(record_size, RECORD_SIZE)

  def test_format_version(self):
    header = build_header(1)
    _, _, _, fmt_ver = struct.unpack(HEADER_FMT, header[:struct.calcsize(HEADER_FMT)])
    self.assertEqual(fmt_ver, FORMAT_VERSION)

  def test_sector_padding(self):
    for n in (0, 1, 999):
      self.assertEqual(len(build_header(n)), SECTOR_SIZE)

  def test_tail_is_zero(self):
    header = build_header(5)
    self.assertEqual(header[struct.calcsize(HEADER_FMT):], b'\x00' * (SECTOR_SIZE - struct.calcsize(HEADER_FMT)))


class TestSDRecord(unittest.TestCase):
  def test_record_size(self):
    msgs = _make_messages(10)
    raw = build_records(msgs)
    self.assertEqual(len(raw) % RECORD_SIZE, 0)
    self.assertEqual(len(raw) // RECORD_SIZE, len(msgs))

  def test_timestamp_first_is_zero(self):
    msgs = _make_messages(5, t0_ns=5_000_000_000)
    raw = build_records(msgs)
    mono_time_us = _unpack_record(raw[:RECORD_SIZE])[0]
    self.assertEqual(mono_time_us, 0)

  def test_timestamp_relative_conversion(self):
    # 1 ms apart in nanoseconds → 1000 µs apart
    msgs = _make_messages(3, t0_ns=0, step_ns=1_000_000)
    raw = build_records(msgs)
    times = [_unpack_record(raw[i * RECORD_SIZE:(i + 1) * RECORD_SIZE])[0] for i in range(3)]
    self.assertEqual(times, [0, 1000, 2000])

  def test_timestamp_overflow_clamp(self):
    # Just beyond uint32 max (in nanoseconds from t0)
    overflow_ns = (0xFFFFFFFF + 1) * _US  # 1 µs past max
    msgs = [(0, 0x100, 0, b'\x00' * 8), (overflow_ns, 0x200, 0, b'\x00' * 8)]
    raw = build_records(msgs)
    clamped = _unpack_record(raw[RECORD_SIZE:])[0]
    self.assertEqual(clamped, 0xFFFFFFFF)

  def test_data_padding_short(self):
    msgs = [(0, 0x100, 0, b'\xAB\xCD')]  # 2 bytes
    raw = build_records(msgs)
    _, _, _, data_len, data, _ = _unpack_record(raw)
    self.assertEqual(data_len, 2)
    self.assertEqual(data, b'\xAB\xCD\x00\x00\x00\x00\x00\x00')

  def test_data_truncation_long(self):
    msgs = [(0, 0x100, 0, b'\xAA' * 12)]  # 12 bytes, must truncate to 8
    raw = build_records(msgs)
    _, _, _, data_len, data, _ = _unpack_record(raw)
    self.assertEqual(data_len, 8)
    self.assertEqual(data, b'\xAA' * 8)

  def test_bus_mask(self):
    msgs = [(0, 0x100, 256, b'\x00' * 8)]  # bus=256 overflows uint8
    raw = build_records(msgs)
    _, _, bus, _, _, _ = _unpack_record(raw)
    self.assertEqual(bus, 0)  # 256 & 0xFF == 0

  def test_pad_field_zero(self):
    msgs = _make_messages(5)
    raw = build_records(msgs)
    for i in range(5):
      pad = _unpack_record(raw[i * RECORD_SIZE:(i + 1) * RECORD_SIZE])[5]
      self.assertEqual(pad, 0)

  def test_sort_order(self):
    # Reverse-order input must produce ascending timestamps in output
    msgs = [(2_000_000, 0x100, 0, b'\x00' * 8),
            (1_000_000, 0x101, 0, b'\x00' * 8),
            (0,         0x102, 0, b'\x00' * 8)]
    raw = build_records(msgs)
    times = [_unpack_record(raw[i * RECORD_SIZE:(i + 1) * RECORD_SIZE])[0] for i in range(3)]
    self.assertEqual(times, sorted(times))
    self.assertEqual(times[0], 0)

  def test_addr_preserved(self):
    msgs = [(0, 0x1FFFFFFF, 0, b'\x00' * 8)]  # max 29-bit address
    raw = build_records(msgs)
    _, addr, _, _, _, _ = _unpack_record(raw)
    self.assertEqual(addr, 0x1FFFFFFF)


class TestSDImage(unittest.TestCase):
  def _write_tmp(self, msgs):
    import os
    records = build_records(msgs)
    header = build_header(len(msgs))
    with tempfile.NamedTemporaryFile(delete=False) as f:
      path = f.name
    write_image(path, header, records)
    self.addCleanup(os.unlink, path)
    return path

  def test_output_sector_aligned(self):
    import os
    for n in (1, 25, 819, 820):  # straddles sector boundary
      path = self._write_tmp(_make_messages(n))
      self.assertEqual(os.path.getsize(path) % SECTOR_SIZE, 0, f"n={n} not sector-aligned")

  def test_header_at_sector_0(self):
    path = self._write_tmp(_make_messages(3))
    with open(path, 'rb') as f:
      sector0 = f.read(SECTOR_SIZE)
    self.assertEqual(sector0[:8], MAGIC)

  def test_records_start_at_sector_1(self):
    msgs = _make_messages(1)
    path = self._write_tmp(msgs)
    with open(path, 'rb') as f:
      f.seek(SECTOR_SIZE)
      raw_record = f.read(RECORD_SIZE)
    mono_time_us, addr, bus, _, _, _ = _unpack_record(raw_record)
    self.assertEqual(mono_time_us, 0)
    self.assertEqual(addr, msgs[0][1])
    self.assertEqual(bus, msgs[0][2])

  def test_round_trip(self):
    msgs = _make_messages(50)
    records_raw = build_records(msgs)
    t0_ns = msgs[0][0]
    for i, (mono_time_ns, addr, bus, data) in enumerate(msgs):
      record = records_raw[i * RECORD_SIZE:(i + 1) * RECORD_SIZE]
      r_time, r_addr, r_bus, r_len, r_data, r_pad = _unpack_record(record)
      expected_us = (mono_time_ns - t0_ns) // _US
      self.assertEqual(r_time, expected_us)
      self.assertEqual(r_addr, addr)
      self.assertEqual(r_bus, bus)
      self.assertEqual(r_len, len(data))
      self.assertEqual(r_data, data)
      self.assertEqual(r_pad, 0)


if __name__ == "__main__":
  unittest.main()
