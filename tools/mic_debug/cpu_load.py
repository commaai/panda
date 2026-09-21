#!/usr/bin/env python3
"""Bounded CPU-only workload without graphics-library dependencies."""
import argparse
import hashlib
import json
import multiprocessing
import os
import time


def worker(core, duration):
  os.sched_setaffinity(0, {core})
  payload = b'\x5a' * 65536
  deadline = time.monotonic() + duration
  try:
    while time.monotonic() < deadline:
      wall_start = time.monotonic()
      cpu_start = time.process_time()
      while time.process_time() - cpu_start < 0.085:
        hashlib.sha256(payload).digest()
      time.sleep(max(0, 0.1 - (time.monotonic() - wall_start)))
  except KeyboardInterrupt:
    pass


def main():
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument('--seconds', type=float, required=True)
  args = parser.parse_args()
  if not 0 < args.seconds <= 180:
    parser.error('seconds must be between 0 and 180')
  cores = sorted(os.sched_getaffinity(0))
  processes = [multiprocessing.Process(target=worker, args=(core, args.seconds)) for core in cores]
  try:
    for process in processes:
      process.start()
    print(json.dumps({'cores': cores, 'target_cpu_percent_per_core': 85, 'seconds': args.seconds}), flush=True)
    for process in processes:
      process.join()
  except KeyboardInterrupt:
    pass
  finally:
    for process in processes:
      if process.is_alive():
        process.terminate()
      process.join()


if __name__ == '__main__':
  main()
