#!/usr/bin/env python3
"""Run an isolated device logger test and restore listening controls on exit."""
import argparse
from pathlib import Path
import shlex
import subprocess
import sys
import uuid

from probe import analyze


def main():
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument('--host', required=True)
  parser.add_argument('--output', type=Path, required=True)
  parser.add_argument('--seconds', type=int, default=60)
  parser.add_argument('--load', action='store_true')
  args = parser.parse_args()
  if args.output.exists():
    parser.error('output already exists')
  args.output.parent.mkdir(parents=True, exist_ok=True)
  tools = Path(__file__).parent
  remote = '/data/mic-lab/logger-' + uuid.uuid4().hex[:12]
  status = [sys.executable, str(tools / 'status.py'), '--host', args.host]
  subprocess.run([*status, '--title', 'Recording through real loggerd', '--message', 'Background capture' + (' with CPU load' if args.load else ''),
                  '--detail', 'No quiet or movement needed. Listening buttons return automatically after the test.',
                  '--duration', str(args.seconds + 15)], check=True)
  try:
    subprocess.run(['ssh', args.host, 'mkdir -m 700 ' + shlex.quote(remote)], check=True)
    subprocess.run(['scp', '-q', str(tools / 'capture.py'), str(tools / 'logger_probe.py'), str(tools / 'cpu_load.py'),
                    args.host + ':' + remote + '/'], check=True)
    command = ['python3', remote + '/logger_probe.py', remote + '/run', '--seconds', str(args.seconds)]
    if args.load:
      command.append('--load')
    result = subprocess.run(['ssh', args.host, 'bash -lc ' + shlex.quote('source /etc/profile && cd /data/openpilot && ' + shlex.join(command))])
    subprocess.run(['scp', '-qr', args.host + ':' + remote + '/run', str(args.output)], check=True)
    if args.output.joinpath('capture.npz').exists():
      analyze(args.output / 'capture')
    result.check_returncode()
  finally:
    subprocess.run([*status, '--title', 'Before/After playback available', '--message', 'Logger test ended; listening buttons enabled.',
                    '--detail', 'No quiet or movement needed. Analysis continues on the PC.', '--allow-playback'], check=True)


if __name__ == '__main__':
  main()
