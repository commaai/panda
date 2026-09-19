#!/usr/bin/env python3
"""Announce, capture, and remotely start an armed iPhone speaker on the same device."""

import argparse
import fcntl
import json
import re
import subprocess
import time
import wave
from pathlib import Path

import numpy as np
import sounddevice as sd

from openpilot.system.micd import patch_sounddevice
from panda import Panda

SPEAKER = Path('/data/mic-lab/phone-sweep-01')


def read_client(client):
  return json.loads((SPEAKER / 'clients' / (client + '.json')).read_text())


def command(client, action, source=None):
  value = {'id': time.monotonic_ns(), 'client': client, 'action': action}
  if source is not None:
    value['source'] = source
  temporary = SPEAKER / 'command.new'
  temporary.write_text(json.dumps(value))
  temporary.replace(SPEAKER / 'command.json')
  return value


def status(message, allowed=False):
  value = {
    'title': 'Automatic iPhone microphone test',
    'message': message,
    'detail': 'Keep the phone still. Playback and recording are controlled together.',
    'allow_playback': allowed,
    'duration': 0,
  }
  temporary = Path('/data/mic-lab/status.json.new')
  temporary.write_text(json.dumps(value))
  temporary.replace('/data/mic-lab/status.json')
  print(message, flush=True)


def cue(path):
  with open('/data/mic-lab/audio.lock', 'a') as lock:
    fcntl.flock(lock, fcntl.LOCK_EX)
    with wave.open(str(path), 'rb') as recording:
      rate = recording.getframerate()
      samples = np.frombuffer(recording.readframes(recording.getnframes()), dtype='<i2')
    sd.play(samples, rate)
    sd.wait()


def main():
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument('output', type=Path)
  parser.add_argument('--source', default='phone-sweep.wav')
  args = parser.parse_args()
  if not re.fullmatch(r'phone-[a-z0-9-]+\.wav', args.source):
    parser.error('Invalid stimulus filename')
  with wave.open(str(SPEAKER / args.source)) as stimulus:
    capture_seconds = stimulus.getnframes() / stimulus.getframerate() + 10
  if not args.output.resolve().is_relative_to('/data'):
    parser.error('Device output must be under /data')
  args.output.mkdir(exist_ok=True)
  if (args.output / 'captured.npz').exists():
    parser.error('Refusing to overwrite an existing recording')
  clients = [json.loads(path.read_text()) for path in (SPEAKER / 'clients').glob('*.json')]
  ready = [
    item
    for item in clients
    if item.get('armed')
    and item.get('visible')
    and item.get('audio_ready')
    and (args.source == 'phone-sweep.wav' or item.get('protocol', 0) >= 2)
    and 'iPhone' in item.get('user_agent', '')
    and time.monotonic() - item.get('received_monotonic', 0) < 4
  ]
  if len(ready) != 1:
    raise RuntimeError(f'Expected one recently armed iPhone, found {len(ready)}')
  client = ready[0]['client']
  (args.output / 'phone_before.json').write_text(json.dumps(ready[0], indent=2) + '\n')
  patch_sounddevice(sd)
  with Panda(cli=False) as panda:
    assert panda.get_signature() == Panda.get_signature_from_firmware('/data/mic-lab/firmware/panda-77581827.bin.signed')
  capture = None
  completed = False
  status('Phone connected. Spoken countdown, then automatic recording.')
  try:
    cue(args.output / 'start.wav')
    latest = read_client(client)
    assert latest['armed'] and latest['audio_ready'] and time.monotonic() - latest['received_monotonic'] < 4
    capture = subprocess.Popen(
      [
        'python3',
        '/data/mic-lab/phone-capture-01/capture.py',
        str(args.output / 'captured'),
        '--seconds',
        str(capture_seconds),
        '--rate',
        '48000',
        '--dtype',
        'int16',
        '--stimulus',
        'none',
      ],
      stdout=subprocess.PIPE,
      stderr=subprocess.STDOUT,
      text=True,
    )
    first = capture.stdout.readline()
    print(first, end='', flush=True)
    if not json.loads(first).get('recording'):
      raise RuntimeError('Capture stream did not report ready')
    play = command(client, 'play', args.source)
    status('Recording: iPhone test audio starts automatically.')
    deadline = time.monotonic() + 5
    while time.monotonic() < deadline:
      latest = read_client(client)
      if latest.get('command_id') == play['id'] and latest.get('playing'):
        break
      time.sleep(0.1)
    else:
      raise RuntimeError('Phone did not acknowledge playback')
    (args.output / 'phone_started.json').write_text(json.dumps(latest, indent=2) + '\n')
    output, _ = capture.communicate(timeout=capture_seconds + 10)
    print(output, flush=True)
    if capture.returncode:
      raise RuntimeError(f'Capture failed: {capture.returncode}')
    latest = read_client(client)
    (args.output / 'phone_after.json').write_text(json.dumps(latest, indent=2) + '\n')
    (args.output / 'playback-command.json').write_text(json.dumps(play, indent=2) + '\n')
    assert latest['armed'] and latest['audio_ready'] and not latest['playing']
    assert latest['command_id'] == play['id'] and time.monotonic() - latest['received_monotonic'] < 4
    completed = True
  finally:
    command(client, 'stop')
    if capture is not None and capture.poll() is None:
      capture.terminate()
      capture.wait(timeout=5)
    if completed:
      status('Recording finished. Phone remains connected for further tests.', allowed=True)
      cue(args.output / 'done.wav')
    else:
      status('Recording aborted. Check phone readiness and the capture log.', allowed=True)


if __name__ == '__main__':
  main()
