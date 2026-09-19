#!/usr/bin/env python3
"""Run mic capture through an isolated real loggerd, optionally under CPU load."""
import argparse
import json
import os
from pathlib import Path
import signal
import shutil
import subprocess
import time

import numpy as np

from openpilot.tools.lib.logreader import LogReader


def stop(process):
  if process is not None and process.poll() is None:
    process.send_signal(signal.SIGINT)
    try:
      process.wait(timeout=10)
    except subprocess.TimeoutExpired:
      process.kill()
      process.wait()


def main():
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument('output', type=Path)
  parser.add_argument('--seconds', type=int, default=60)
  parser.add_argument('--load', action='store_true')
  args = parser.parse_args()
  if not args.output.resolve().is_relative_to('/data'):
    parser.error('output must be under /data')
  if not 1 <= args.seconds <= 120:
    parser.error('seconds must be between 1 and 120')
  args.output.mkdir(parents=True, exist_ok=False)
  ipc_prefix = args.output.parent.name + '_' + args.output.name
  environment = {**os.environ, 'PARAMS_ROOT': str(args.output / 'params'), 'LOG_ROOT': str(args.output / 'logs'),
                 'OPENPILOT_PREFIX': ipc_prefix, 'COMMA_CACHE': str(args.output / 'cache'),
                 'LOGGERD_TEST': '1', 'LOGGERD_SEGMENT_LENGTH': '60'}
  ipc_directory = Path('/dev/shm') / ('msgq_' + ipc_prefix)
  ipc_directory.mkdir(mode=0o700)
  subprocess.run(['python3', '-c', 'from openpilot.common.params import Params; Params().put_bool("RecordAudio", True)'],
                 env=environment, check=True)
  logger = None
  stress = None
  capture = None
  monitor = []
  with (args.output / 'logger.log').open('w') as logger_log, (args.output / 'stress.log').open('w') as stress_log:
    try:
      logger = subprocess.Popen(['/data/openpilot/openpilot/system/loggerd/loggerd'], env=environment,
                                stdout=logger_log, stderr=subprocess.STDOUT)
      time.sleep(1)
      if logger.poll() is not None:
        raise RuntimeError('loggerd exited before capture; inspect logger.log')
      if args.load:
        stress = subprocess.Popen(['python3', str(Path(__file__).with_name('cpu_load.py')), '--seconds', str(args.seconds + 5)],
                                   stdout=stress_log, stderr=subprocess.STDOUT)
      capture = subprocess.Popen(['python3', str(Path(__file__).with_name('capture.py')), str(args.output / 'capture'), '--seconds', str(args.seconds),
                                  '--workload', 'micd-ipc', '--replay', '--replay-seconds', '10'], env=environment)
      while capture.poll() is None:
        temperature = int(Path('/sys/class/thermal/thermal_zone0/temp').read_text())
        monitor.append({'monotonic': time.monotonic(), 'zone0_millidegrees': temperature,
                        'cpu_stat': Path('/proc/stat').read_text().splitlines()[0],
                        'loadavg': Path('/proc/loadavg').read_text().strip()})
        if temperature >= 75000 and stress is not None and stress.poll() is None:
          stop(stress)
          print('Stopped CPU load at thermal threshold', flush=True)
        if stress is not None and stress.poll() not in (None, 0):
          raise RuntimeError('CPU workload failed; this is not a valid load test')
        time.sleep(1)
      if capture.returncode:
        raise RuntimeError(f'capture exited {capture.returncode}')
      time.sleep(0.5)
    finally:
      stop(capture)
      stop(stress)
      stop(logger)
      shutil.rmtree(ipc_directory)
      (args.output / 'monitor.json').write_text(json.dumps(monitor, indent=2) + '\n')
  chunks = []
  timestamps = []
  for path in sorted((args.output / 'logs').glob('*/rlog.zst')):
    for message in LogReader(str(path)):
      if message.which() == 'rawAudioData':
        chunks.append(bytes(message.rawAudioData.data))
        timestamps.append(message.logMonoTime)
  saved = np.load(args.output / 'capture.npz')
  expected = saved['logged_samples'].astype('<i2').tobytes()
  actual = b''.join(chunks)
  result = {'audio_messages': len(chunks), 'logged_frames': len(actual)//2, 'expected_frames': len(expected)//2,
            'real_logger_pcm_bit_exact': actual == expected, 'logger_returncode': logger.returncode,
            'stress_returncode': stress.returncode if stress is not None else None,
            'scope': 'Actual Mic.callback, real IPC and loggerd; isolated params/log paths. No cameras, CAN, modeld or video muxing.',
            'message_interval_ms': np.percentile(np.diff(timestamps)/1e6, [0,50,99,100]).tolist() if len(timestamps)>1 else []}
  (args.output / 'logger_check.json').write_text(json.dumps(result, indent=2) + '\n')
  print(json.dumps(result), flush=True)
  assert actual == expected, 'Logger PCM does not exactly match captured callback output'


if __name__ == '__main__':
  main()
