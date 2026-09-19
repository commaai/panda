import base64
import io
import json
import subprocess
import wave
from pathlib import Path

import numpy as np

RESULTS = Path('/home/batman/tmp/c4-mic-results')
SOURCE = RESULTS / 'phone-music-camera-01/captured.wav'
FFMPEG = '/usr/bin/ffmpeg'
RATE = 16000


def measure(samples):
  result = subprocess.run([FFMPEG, '-hide_banner', '-f', 'f64le', '-ar', str(RATE), '-ac', '1', '-i', 'pipe:0', '-af', 'loudnorm=I=-24:TP=-3:print_format=json', '-f', 'null', '-'], input=samples.astype('<f8').tobytes(), capture_output=True, check=True)
  output = result.stderr.decode()
  values = json.loads(output[output.rfind('{'):output.rfind('}')+1])
  return {key: float(value) for key, value in values.items() if key.startswith('input_')}


for directory in ('clarity-comparison-01', 'reference-eq-01'):
  root = RESULTS / directory
  encoded_path = root / 'logged-equivalent.m4a'
  subprocess.run([FFMPEG, '-v', 'error', '-y', '-i', str(SOURCE), '-ss', '3.48', '-t', '45', '-ar', str(RATE), '-ac', '1', '-c:a', 'aac', '-b:a', '32000', str(encoded_path)], check=True)
  raw = subprocess.check_output([FFMPEG, '-v', 'error', '-i', str(encoded_path), '-f', 'f64le', '-ar', str(RATE), '-ac', '1', '-'])
  samples = np.frombuffer(raw, dtype='<f8')[:45 * RATE]
  assert len(samples) == 45 * RATE
  metadata = json.loads((root / 'metrics.json').read_text())
  target = metadata['loudness_target_lufs']
  before = measure(samples)
  gain = 10 ** ((target - before['input_i']) / 20)
  playback = samples * gain
  assert abs(playback).max() < .98
  buffer = io.BytesIO()
  with wave.open(buffer, 'wb') as audio:
    audio.setparams((1, 2, RATE, 0, 'NONE', 'not compressed'))
    audio.writeframes(np.rint(playback * 32767).astype('<i2').tobytes())
  data = buffer.getvalue()
  (root / 'logged-equivalent.wav').write_bytes(data)
  decoded = np.frombuffer(data[44:], dtype='<i2').astype(float) / 32768
  after = measure(decoded)
  assert abs(after['input_i'] - target) < .15
  probe = json.loads(subprocess.check_output(['/usr/bin/ffprobe', '-v', 'error', '-select_streams', 'a:0', '-show_entries', 'stream=codec_name,profile,sample_rate,channels,bit_rate,duration', '-of', 'json', str(encoded_path)]))['streams'][0]
  assert probe['codec_name'] == 'aac' and int(probe['sample_rate']) == RATE and probe['channels'] == 1
  note = {'title': 'Logged equivalent — 16 kHz AAC, 32 kbps', 'codec': probe, 'integrated_lufs': after['input_i'], 'true_peak_dbfs': after['input_tp'], 'gain_after_decode_db': float(20*np.log10(gain)), 'scope': 'Same native 48 kHz capture reduced to 16 kHz mono and encoded AAC at 32 kbit/s, then decoded for embedded playback. Approximation of current Connect/video format, not actual old-firmware capture or exact on-device resampling/encoder equivalence. Existing clock/startup clicks are not recreated. Raw rlogs use 16 kHz PCM; qlogs contain no audio. Native 48 kHz here is experimental capture; production micd still defaults to 16 kHz.'}
  metadata['logged_equivalent'] = note
  (root / 'metrics.json').write_text(json.dumps(metadata, indent=2) + '\n')
  (root / 'logged-equivalent.json').write_text(json.dumps(note, indent=2) + '\n')
  page = (root / 'index.html').read_text()
  start = page.index('const clips=') + len('const clips=')
  end = page.index(';const player=', start)
  clips = json.loads(page[start:end])
  clips.pop('logged', None)
  clips = {'logged': {'title': note['title'], 'uri': 'data:audio/wav;base64,' + base64.b64encode(data).decode()}, **clips}
  page = page[:start] + json.dumps(clips) + page[end:]
  if 'id="logged-note"' not in page:
    page = page.replace('</h1>', '</h1><p id="logged-note"><strong>Logged equivalent</strong> approximates the current Connect/video audio format: the same recording downsampled to 16 kHz mono and AAC-encoded at 32 kbps, then decoded for playback. It is loudness-matched and does not recreate the old firmware clicks. “Original” is our native 48 kHz experimental microphone capture; production logging still defaults to 16 kHz.</p>', 1)
  (root / 'index.html').write_text(page)
  print(json.dumps({'report': directory, **note}))
