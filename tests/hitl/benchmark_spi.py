#!/usr/bin/env python3
"""Compare SPI backends on an idle CI device (requires spidev for the baseline).

Run with exclusive access to the panda: python tests/hitl/benchmark_spi.py
Each round alternates backend order and checks responses. Normal protocol retries
are counted, including the NACKs used by the firmware for flow control.
"""
import argparse
from collections import Counter
import json
import os
import statistics
import time

from opendbc.car.structs import CarParams
from panda import Panda
from panda.python import spi
from panda.python.spi import SpiDev


def legacy_device(path, speed):
  import spidev
  device = spidev.SpiDev()
  device.open_path(path)
  device.max_speed_hz = speed
  return device


def select_backend(factory):
  for device in spi.SPI_DEVICES.values():
    device.close()
  spi.SPI_DEVICES.clear()
  spi.SpiDev = factory


def measure(operation, count):
  for _ in range(20):
    operation()
  samples = []
  cpu_start = time.process_time_ns()
  for _ in range(count):
    start = time.perf_counter_ns()
    operation()
    samples.append((time.perf_counter_ns() - start) / 1000)
  cpu_us = (time.process_time_ns() - cpu_start) / count / 1000
  return {'median_us': statistics.median(samples), 'p95_us': sorted(samples)[int(0.95 * (count - 1))],
          'mean_us': statistics.mean(samples), 'cpu_us': cpu_us}


def benchmark(factory, iterations):
  select_backend(factory)
  with Panda() as panda:
    assert panda.spi, 'This benchmark must use SPI, not USB'
    panda.reset()
    panda._handle.no_retry = False
    panda.set_power_save(False)
    panda.set_safety_mode(CarParams.SafetyModel.allOutput)
    panda.set_can_loopback(True)
    for bus in range(3):
      panda.set_can_speed_kbps(bus, 1000)
      panda.can_clear(bus)
    panda.can_clear(0xFFFF)
    while panda.can_recv():
      pass
    uid = panda.get_uid()
    version = panda._handle.get_protocol_version()
    errors = panda.health()['spi_error_count']

    def health():
      result = panda.health()
      assert result['faults'] == 0

    def get_uid():
      assert panda.get_uid() == uid

    def protocol_version():
      assert panda._handle.get_protocol_version() == version

    def empty_can():
      assert panda.can_recv() == []

    def loopback(count):
      messages = [(0x100 + i, bytes((i + j) % 256 for j in range(8)), i % 3) for i in range(count)]
      expected = Counter((addr, data, bus) for addr, data, bus in messages)
      expected.update((addr, data, bus | 0x80) for addr, data, bus in messages)

      def transfer():
        panda.can_send_many(messages)
        received = []
        deadline = time.monotonic() + 2
        while len(received) < count * 2 and time.monotonic() < deadline:
          received.extend(panda.can_recv())
        assert Counter((addr, bytes(data), bus) for addr, data, bus in received) == expected
      return transfer

    retries = Counter()
    transfer = panda._handle._transfer_spidev

    def counted_transfer(*args, **kwargs):
      try:
        return transfer(*args, **kwargs)
      except spi.PandaSpiException as e:
        retries[type(e).__name__] += 1
        raise

    panda._handle._transfer_spidev = counted_transfer
    try:
      operations = {'health': health, 'uid': get_uid, 'protocol_version': protocol_version, 'empty_can': empty_can,
                    'can_loopback_1': loopback(1), 'can_loopback_200': loopback(200)}
      results = {}
      for name, operation in operations.items():
        print(f'benchmark {factory.__name__}: {name}', flush=True)
        before = retries.copy()
        results[name] = measure(operation, iterations if name != 'can_loopback_200' else max(20, iterations // 20))
        results[name]['retries'] = dict(retries - before)
      results['spi_error_delta'] = panda.health()['spi_error_count'] - errors
      return results
    finally:
      panda.set_can_loopback(False)
      panda.set_safety_mode(CarParams.SafetyModel.silent)


def main():
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument('--rounds', type=int, default=6)
  parser.add_argument('--iterations', type=int, default=1000)
  parser.add_argument('--cpu', type=int, help='Pin both backends to the same CPU for comparison')
  parser.add_argument('--output', default='spi_benchmark.json')
  args = parser.parse_args()
  if args.cpu is not None:
    os.sched_setaffinity(0, {args.cpu})
  results = []
  try:
    for round_index in range(args.rounds):
      backends = [('legacy', legacy_device), ('native', SpiDev)]
      for name, factory in backends[::1 if round_index % 2 == 0 else -1]:
        result = {'round': round_index, 'backend': name, 'cpus': sorted(os.sched_getaffinity(0)), 'measurements': benchmark(factory, args.iterations)}
        results.append(result)
        print(json.dumps(result), flush=True)
        with open(args.output, 'w') as f:
          json.dump(results, f, indent=2)
  finally:
    select_backend(SpiDev)


if __name__ == '__main__':
  main()
