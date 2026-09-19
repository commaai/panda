#!/usr/bin/env python3
"""Record gross device motion alongside an audio probe, with sensord stopped."""
import argparse
import json
from pathlib import Path
import time

import numpy as np

from openpilot.system.sensord.sensord import I2C_BUS_IMU
from openpilot.system.sensord.sensors.lsm6ds3_accel import LSM6DS3_Accel


def main():
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument('output', type=Path)
  parser.add_argument('--seconds', type=float, default=75)
  args = parser.parse_args()
  if not 0 < args.seconds <= 600:
    parser.error('seconds must be between 0 and 600')
  if not args.output.resolve().is_relative_to('/data'):
    parser.error('device recordings must be stored under /data')
  for process in Path('/proc').iterdir():
    if process.name.isdecimal():
      try:
        command = (process / 'cmdline').read_bytes().replace(b'\0', b' ')
      except OSError:
        continue
      if b'sensord' in command and (b'python' in command or command.startswith(b'sensord')):
        parser.error('sensord is running; stop it before direct sensor access')

  sensor = LSM6DS3_Accel(I2C_BUS_IMU)
  registers = [sensor.LSM6DS3_ACCEL_I2C_REG_CTRL3_C, sensor.LSM6DS3_ACCEL_I2C_REG_DRDY_CFG,
               sensor.LSM6DS3_ACCEL_I2C_REG_INT1_CTRL, sensor.LSM6DS3_ACCEL_I2C_REG_CTRL1_XL]
  saved = {register: sensor.read(register, 1)[0] for register in registers}
  rows = []
  error = None
  try:
    sensor.init()
    started = time.monotonic()
    print(json.dumps({'motion_recording': True, 'started_monotonic': started}), flush=True)
    while time.monotonic() - started < args.seconds:
      timestamp = time.monotonic_ns()
      try:
        event = sensor.get_event(timestamp)
        rows.append([timestamp / 1e9, *event.acceleration.v])
      except sensor.DataNotReady:
        pass
      time.sleep(0.003)
  except Exception as exception:
    error = repr(exception)
    raise
  finally:
    for register in registers:
      sensor.write(register, saved[register])
    restored = {register: sensor.read(register, 1)[0] for register in registers}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    np.savez(args.output.with_suffix('.npz'), acceleration=np.asarray(rows, dtype=np.float64).reshape(-1, 4))
    metadata = {'columns': ['monotonic_seconds', 'x_mps2', 'y_mps2', 'z_mps2'], 'samples': len(rows),
                'requested_seconds': args.seconds, 'saved_registers': saved, 'restored_registers': restored,
                'registers_restored': restored == saved, 'error': error,
                'scope': '104 Hz, +/-2g; polled read timestamps, not hardware IRQ timestamps. Gross movement evidence, not broadband vibration measurement.'}
    args.output.with_suffix('.json').write_text(json.dumps(metadata, indent=2) + '\n')
    print(json.dumps(metadata), flush=True)


if __name__ == '__main__':
  main()
