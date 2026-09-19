"""Capture an external headphone source, with spoken cues on comma four."""
import json
import subprocess
import sys
from pathlib import Path

import sounddevice as sd
from openpilot.system.micd import patch_sounddevice
from panda import Panda

sys.path.insert(0, '/data/mic-lab/phone-sweep-01')
from record import cue

ROOT = Path('/data/mic-lab/headphone-source-01')


def status(message):
  value = {'title': 'Independent headphone source test', 'message': message,
           'detail': 'Native 48 kHz microphone capture. Keep the earcup still.',
           'allow_playback': False, 'duration': 45}
  temporary = Path('/data/mic-lab/status.json.new')
  temporary.write_text(json.dumps(value))
  temporary.replace('/data/mic-lab/status.json')


def main():
  if (ROOT / 'captured.npz').exists():
    raise RuntimeError('Refusing to overwrite existing recording')
  patch_sounddevice(sd)
  with Panda(cli=False) as panda:
    if panda.get_signature() != Panda.get_signature_from_firmware('/data/mic-lab/firmware/panda-77581827.bin.signed'):
      raise RuntimeError('Unexpected firmware')
  status('Spoken countdown, then external headphone tones.')
  cue(ROOT / 'start.wav')
  capture = subprocess.Popen(['python3', '/data/mic-lab/phone-capture-01/capture.py',
                              str(ROOT / 'captured'), '--seconds', '45', '--rate', '48000',
                              '--dtype', 'int16', '--stimulus', 'none'],
                             stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
  try:
    first = capture.stdout.readline()
    if not json.loads(first).get('recording'):
      raise RuntimeError('Capture did not become ready: ' + first)
    status('Recording: headphones playing spaced tones from 1 to 22 kHz.')
    print(first, end='', flush=True)
    output, _ = capture.communicate(timeout=65)
    print(output, flush=True)
    if capture.returncode:
      raise RuntimeError('Capture failed')
  finally:
    if capture.poll() is None:
      capture.terminate()
      capture.wait(timeout=5)
  status('Recording finished. Analyzing the external source.')
  cue(ROOT / 'done.wav')


if __name__ == '__main__':
  main()
