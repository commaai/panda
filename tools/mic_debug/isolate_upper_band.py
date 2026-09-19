#!/usr/bin/env python3
"""Make listening clips of the native capture above 16 kHz."""

import argparse
import base64
import hashlib
import json
import subprocess
import wave
from pathlib import Path

import numpy as np


def write_wav(path, samples, rate):
  subprocess.run(
    ['/usr/bin/ffmpeg', '-v', 'error', '-y', '-f', 'f64le', '-ar', str(rate), '-ac', '1', '-i', '-', '-c:a', 'pcm_s24le', str(path)],
    input=np.asarray(samples, dtype='<f8').tobytes(),
    check=True,
  )


def main():
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument('source', type=Path)
  parser.add_argument('output', type=Path)
  args = parser.parse_args()
  args.output.mkdir(parents=True, exist_ok=True)
  with wave.open(str(args.source)) as recording:
    rate = recording.getframerate()
    assert rate == 48000 and recording.getsampwidth() == 2 and recording.getnchannels() == 1
    source = np.frombuffer(recording.readframes(recording.getnframes()), dtype='<i2').astype(float) / 32768
  padded = np.pad(source, (rate, rate), mode='reflect')
  frequencies = np.fft.rfftfreq(len(padded), 1 / rate)
  transition = np.clip((frequencies - 16000) / 250, 0, 1)
  response = 0.5 - 0.5 * np.cos(np.pi * transition)
  filtered = np.fft.irfft(np.fft.rfft(padded) * response, n=len(padded))[rate:-rate]
  fade_length = round(0.02 * rate)
  ramp = 0.5 - 0.5 * np.cos(np.linspace(0, np.pi, fade_length))
  filtered[:fade_length] *= ramp
  filtered[-fade_length:] *= ramp[::-1]
  rms = float(np.sqrt(np.mean(filtered**2)))
  gain = min(10 ** (-35 / 20) / rms, 0.1 / np.max(np.abs(filtered)))
  amplified = filtered * gain
  excerpt = amplified[: 8 * rate].copy()
  excerpt[-fade_length:] *= ramp[::-1]
  variants = [
    ('above16_original_level.wav', 'Above 16 kHz — original captured level', filtered, rate),
    ('above16_amplified.wav', f'Above 16 kHz — amplified {20 * np.log10(gain):.1f} dB, original pitch', amplified, rate),
    ('above16_slowed.wav', 'Amplified excerpt — quarter speed: 16–24 kHz becomes 4–6 kHz', excerpt, rate // 4),
  ]
  page = '''<!doctype html><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Above 16 kHz microphone capture</title><style>
body{font:17px/1.5 system-ui;max-width:900px;margin:30px auto;padding:0 20px}audio{width:100%}
section{border:1px solid #ccc;padding:16px;margin:16px 0}h2{font-size:20px}
</style><h1>Isolated audio above 16 kHz</h1>
<p>A high-pass filter keeps high frequencies; a low-pass filter keeps low frequencies.
These clips come from the same real native 48 kHz music recording. No new recording or simulated noise was used.</p>
<p>The offline filter rejects frequencies below 16 kHz and transitions to full gain at 16.25 kHz.
The first player preserves the captured level. The second amplifies the faint band without changing pitch.
The third slows the first eight seconds to 32 seconds, lowering the band to 4–6 kHz so its texture is easier to hear.
That third player is a diagnostic transformation, not what the original recording sounds like.</p>
<p>Original-pitch content may be difficult to hear even when amplified. Use the slowed version instead of turning up playback volume.
Players do not autoplay. The amplified files target −35 dBFS RMS and remain below −20 dBFS peak.</p>
'''
  metadata = {
    'source_sha256': hashlib.sha256(args.source.read_bytes()).hexdigest(),
    'filter': 'Reflect-padded FFT high-pass; zero through 16 kHz; raised-cosine transition to unity at 16.25 kHz.',
    'edge_fades_seconds': 0.02,
    'isolated_rms_dbfs': float(20 * np.log10(rms)),
    'amplification_db': float(20 * np.log10(gain)),
    'variants': [],
  }
  for filename, label, samples, sample_rate in variants:
    assert np.max(np.abs(samples)) <= 0.10000001
    path = args.output / filename
    write_wav(path, samples, sample_rate)
    decoded = subprocess.run(['/usr/bin/ffmpeg', '-v', 'error', '-i', str(path), '-f', 'f64le', '-'], capture_output=True, check=True)
    recovered = np.frombuffer(decoded.stdout, dtype='<f8')
    assert len(recovered) == len(samples)
    assert np.max(np.abs(recovered - samples)) < 2**-22
    uri = 'data:audio/wav;base64,' + base64.b64encode(path.read_bytes()).decode()
    page += f'<section><h2>{label}</h2><audio controls preload="none" src="{uri}"></audio></section>'
    metadata['variants'].append(
      {
        'file': filename,
        'sample_rate': sample_rate,
        'duration_seconds': len(samples) / sample_rate,
        'rms_dbfs': float(20 * np.log10(np.sqrt(np.mean(samples**2)))),
      }
    )
  spectrum = abs(np.fft.rfft(filtered)) ** 2
  bins = np.fft.rfftfreq(len(filtered), 1 / rate)
  metadata['energy_fraction_below_15500_hz_after_edge_fades'] = float(spectrum[bins < 15500].sum() / spectrum.sum())
  assert metadata['energy_fraction_below_15500_hz_after_edge_fades'] < 1e-6
  page += '''<script>document.querySelectorAll('audio').forEach(a=>a.addEventListener('play',()=>{
    document.querySelectorAll('audio').forEach(b=>{if(a!==b)b.pause();});
  }));</script>'''
  (args.output / 'index.html').write_text(page)
  (args.output / 'metrics.json').write_text(json.dumps(metadata, indent=2) + '\n')
  print(json.dumps(metadata))


if __name__ == '__main__':
  main()
