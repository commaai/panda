#!/usr/bin/env python3
"""
Provision a panda jungle v2 SD card for CAN replay.

Reads CAN messages from an openpilot rlog/qlog file and writes them to an
SD card (or binary file) in the panda jungle raw replay format.

Usage:
  provision_sd.py <rlog_or_qlog> <sd_device_or_output_file>

Examples:
  # Write directly to an SD card block device (requires root/sudo on Linux):
  provision_sd.py /data/media/0/realdata/abc123--0/0/rlog /dev/sdb

  # Write to a binary file, then dd to the SD card manually:
  provision_sd.py route.rlog replay.bin
  dd if=replay.bin of=/dev/sdb bs=512

SD card binary format written by this script:
  Sector 0 (512 bytes): header
    bytes  0-7:  magic b"PNDREPLY"
    bytes  8-11: uint32 num_records
    bytes 12-15: uint32 record_size  (= 20)
    bytes 16-19: uint32 format_version  (= 1)
    bytes 20-511: zero-padded
  Sectors 1+: records (20 bytes each)
    uint32 mono_time_us   - microseconds since replay start (relative)
    uint32 addr           - CAN address
    uint8  bus            - CAN bus number (0-2)
    uint8  len            - data byte length (0-8)
    uint8  data[8]        - CAN payload (zero-padded)
    uint16 pad            - reserved (0)
"""

import argparse
import struct
import sys

MAGIC = b"PNDREPLY"
FORMAT_VERSION = 1
RECORD_SIZE = 20
SECTOR_SIZE = 512
HEADER_SECTOR = 0
DATA_START_SECTOR = 1

HEADER_FMT = "<8sIII"   # magic, num_records, record_size, format_version
RECORD_FMT = "<II BB 8s H"  # mono_time_us, addr, bus, len, data[8], pad


def parse_args():
  parser = argparse.ArgumentParser(description="Provision panda jungle v2 SD card for CAN replay")
  parser.add_argument("rlog", help="Path to openpilot rlog or qlog file")
  parser.add_argument("output", help="SD card block device (e.g. /dev/sdb) or output binary file")
  return parser.parse_args()


def read_can_messages(rlog_path):
  """Read CAN messages from an openpilot rlog/qlog file.

  Returns list of (mono_time_ns, addr, bus, data) tuples sorted by time.
  """
  try:
    from openpilot.tools.lib.logreader import LogReader
  except ImportError:
    try:
      from tools.lib.logreader import LogReader
    except ImportError:
      print("Error: could not import LogReader. Run from an openpilot checkout or install openpilot tools.", file=sys.stderr)
      sys.exit(1)

  messages = []
  lr = LogReader(rlog_path)
  for msg in lr:
    if msg.which() == "sendcan":
      for can_msg in msg.sendcan:
        messages.append((msg.logMonoTime, can_msg.address, can_msg.src, bytes(can_msg.dat)))

  if not messages:
    print("Error: no sendcan messages found in log file.", file=sys.stderr)
    sys.exit(1)

  messages.sort(key=lambda m: m[0])
  return messages


def build_records(messages):
  """Convert (mono_time_ns, addr, bus, data) list to binary record bytes."""
  t0_ns = messages[0][0]
  records = bytearray()
  for mono_time_ns, addr, bus, data in messages:
    elapsed_us = (mono_time_ns - t0_ns) // 1000
    if elapsed_us > 0xFFFFFFFF:
      print(f"Warning: timestamp overflow at {elapsed_us} us, clamping to 32-bit max.", file=sys.stderr)
      elapsed_us = 0xFFFFFFFF

    data_len = min(len(data), 8)
    padded_data = data[:data_len].ljust(8, b'\x00')

    record = struct.pack(RECORD_FMT, elapsed_us, addr, bus & 0xFF, data_len, padded_data, 0)
    assert len(record) == RECORD_SIZE, f"record size mismatch: {len(record)}"
    records.extend(record)

  return records


def build_header(num_records):
  header_data = struct.pack(HEADER_FMT, MAGIC, num_records, RECORD_SIZE, FORMAT_VERSION)
  # Pad to one full sector
  return header_data + b'\x00' * (SECTOR_SIZE - len(header_data))


def write_image(output_path, header, records):
  with open(output_path, 'wb') as f:
    f.write(header)
    f.write(records)
    # Pad final partial sector with zeros
    remainder = len(records) % SECTOR_SIZE
    if remainder != 0:
      f.write(b'\x00' * (SECTOR_SIZE - remainder))


def main():
  args = parse_args()

  print(f"Reading CAN messages from {args.rlog}...")
  messages = read_can_messages(args.rlog)
  print(f"  Found {len(messages)} sendcan messages")

  records = build_records(messages)
  num_records = len(messages)

  duration_s = (messages[-1][0] - messages[-1][0]) // 1_000_000_000 if len(messages) > 1 else 0
  elapsed_us = (messages[-1][0] - messages[0][0]) // 1000
  duration_s = elapsed_us / 1_000_000

  header = build_header(num_records)

  total_bytes = len(header) + len(records)
  total_sectors = (total_bytes + SECTOR_SIZE - 1) // SECTOR_SIZE

  print(f"  Replay duration: {duration_s:.1f} seconds")
  print(f"  Total records:   {num_records}")
  print(f"  SD image size:   {total_sectors} sectors ({total_bytes / 1024:.1f} KB)")

  print(f"Writing to {args.output}...")
  write_image(args.output, header, records)
  print("Done. Insert SD card into jungle v2 and call sd_replay_start() to begin replay.")


if __name__ == "__main__":
  main()
