#!/usr/bin/env python3
"""Update the optional microphone investigation displays and desktop notification."""
import argparse
import json
import shlex
import subprocess
import time
from pathlib import Path


def main():
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument('--host')
  parser.add_argument('--file', type=Path, default=Path('/tmp/comma-mic/status.json'))
  parser.add_argument('--remote-file', default='/data/mic-lab/status.json')
  parser.add_argument('--title', required=True)
  parser.add_argument('--message', required=True)
  parser.add_argument('--detail', default='')
  parser.add_argument('--duration', type=float, default=0)
  parser.add_argument('--notify', action='store_true')
  parser.add_argument('--allow-playback', action='store_true')
  args = parser.parse_args()
  status = {key: getattr(args, key) for key in ('title', 'message', 'detail', 'duration', 'allow_playback')}
  status['started'] = time.time()  # noqa: TID251 - Shared wall-clock timestamps across the PC and device.
  args.file.parent.mkdir(parents=True, exist_ok=True)
  temporary = args.file.with_suffix('.tmp')
  temporary.write_text(json.dumps(status) + '\n')
  temporary.replace(args.file)
  if args.host:
    subprocess.run(['scp', '-q', str(args.file), args.host + ':' + shlex.quote(args.remote_file)], check=True)
  if args.notify:
    subprocess.run(['notify-send', '-t', '7000', args.title, args.message], check=True)


if __name__ == '__main__':
  main()
