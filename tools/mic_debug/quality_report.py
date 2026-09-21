#!/usr/bin/env python3
"""Compare offline rate, codec and denoising choices using one native mic WAV."""

import argparse
import base64
import hashlib
import html
import io
import json
import subprocess
import wave
from pathlib import Path

import matplotlib

matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np
from matplotlib.mlab import psd


def read_wav(path):
  with wave.open(str(path), 'rb') as recording:
    assert recording.getsampwidth() == 2 and recording.getnchannels() == 1
    return recording.getframerate(), np.frombuffer(recording.readframes(recording.getnframes()), dtype='<i2').astype(float) / 32768


def wav_bytes(samples, rate):
  assert np.max(np.abs(samples)) < 1
  buffer = io.BytesIO()
  with wave.open(buffer, 'wb') as recording:
    recording.setparams((1, 2, rate, 0, 'NONE', 'not compressed'))
    recording.writeframes(np.rint(samples * 32767).astype('<i2').tobytes())
  return buffer.getvalue()


def main():
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument('source', type=Path)
  parser.add_argument('output', type=Path)
  parser.add_argument('--description', default='Native microphone capture with the local firmware fix.')
  args = parser.parse_args()
  args.output.mkdir(parents=True, exist_ok=True)
  rate, source = read_wav(args.source)
  assert rate == 48000
  duration = len(source) / rate
  variants = [
    ('native48', 'Native 48 kHz PCM', 48000, None, None),
    ('pcm16', '16 kHz PCM: sample-rate reduction only', 16000, None, None),
    ('aac16_32', '16 kHz AAC, 32 kbit/s', 16000, '32k', None),
    ('aac16_96', '16 kHz AAC, 96 kbit/s', 16000, '96k', None),
    ('aac48_96', '48 kHz AAC, 96 kbit/s', 48000, '96k', None),
    ('denoise48', '48 kHz PCM with mild experimental denoising', 48000, None, 'afftdn=nr=6:nf=-60:gs=5'),
  ]
  processed = []
  commands = []
  for key, title, output_rate, bitrate, noise_filter in variants:
    path = args.source if key == 'native48' else args.output / (key + '.wav')
    if key != 'native48':
      output = args.output / (key + '.m4a') if bitrate else path
      command = ['/usr/bin/ffmpeg', '-y', '-hide_banner', '-loglevel', 'error', '-i', str(args.source), '-map_metadata', '-1', '-ar', str(output_rate)]
      if noise_filter:
        command.extend(['-af', noise_filter])
      command.extend(['-c:a', 'aac', '-b:a', bitrate] if bitrate else ['-c:a', 'pcm_s16le'])
      command.append(str(output))
      subprocess.run(command, check=True)
      commands.append(command)
      if bitrate:
        command = [
          '/usr/bin/ffmpeg',
          '-y',
          '-hide_banner',
          '-loglevel',
          'error',
          '-i',
          str(output),
          '-t',
          str(duration),
          '-map_metadata',
          '-1',
          '-c:a',
          'pcm_s16le',
          str(path),
        ]
        subprocess.run(command, check=True)
        commands.append(command)
    actual_rate, signal = read_wav(path)
    assert actual_rate == output_rate and len(signal) == round(duration * actual_rate)
    processed.append((key, title, actual_rate, signal, path))
  gain = min(128, 0.7 / max(np.max(np.abs(item[3])) for item in processed))
  figure, axis = plt.subplots(figsize=(10, 5), constrained_layout=True)
  rows = []
  cards = []
  metadata = {
    'source_sha256': hashlib.sha256(args.source.read_bytes()).hexdigest(),
    'duration_seconds': duration,
    'playback_gain': gain,
    'variants': [],
    'commands': commands,
  }
  for key, title, actual_rate, signal, _path in processed:
    power, frequencies = psd(signal[actual_rate:], Fs=actual_rate, NFFT=actual_rate // 4, noverlap=actual_rate // 8, detrend="mean")
    axis.plot(frequencies, 10 * np.log10(np.maximum(power, 1e-20)), label=title)
    pcm = wav_bytes(signal * gain, actual_rate)
    (args.output / (key + '_listen.wav')).write_bytes(pcm)
    audio = 'data:audio/wav;base64,' + base64.b64encode(pcm).decode()
    cards.append(f'<section><h2>{html.escape(title)}</h2><audio controls preload="none" src="{audio}"></audio></section>')
    peak = float(np.max(np.abs(signal)))
    rms = float(np.std(signal))
    near_rails = int(np.sum(np.abs(signal) >= 0.999))
    metrics = {
      'variant': key,
      'sample_rate': actual_rate,
      'rms_dbfs': float(20 * np.log10(max(rms, 1e-15))),
      'peak_dbfs': float(20 * np.log10(max(peak, 1e-15))),
      'near_rail_samples': near_rails,
    }
    metadata['variants'].append(metrics)
    rows.append(f'<tr><td>{html.escape(title)}</td><td>{metrics["rms_dbfs"]:.1f}</td><td>{metrics["peak_dbfs"]:.1f}</td><td>{near_rails}</td></tr>')
  axis.set(xlabel='Frequency (Hz)', ylabel='Power spectral density (dBFS/Hz)', title='One real recording, different offline processing', xlim=(0, 24000))
  axis.legend(fontsize='small')
  figure.savefig(args.output / 'spectrum.png')
  plt.close(figure)
  image = 'data:image/png;base64,' + base64.b64encode((args.output / 'spectrum.png').read_bytes()).decode()
  page = '''<!doctype html><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Microphone quality baseline</title>
<style>
body{font:17px/1.5 system-ui;max-width:1000px;margin:30px auto;padding:0 20px}audio,img{width:100%}section{border:1px solid
#ccc;padding:16px;margin:16px 0}h2{font-size:20px}td,th{text-align:left;padding:8px}table{width:100%}pre{white-space:pre-wrap}
</style>
<h1>Microphone quality: one recording, six processing choices</h1>
<p>The reference is a new DURATION_SECONDS-second native 48 kHz mono recording using the microphone firmware fix. Every version uses the same captured sound.
The full source is preserved separately. Listening players apply one shared constant gain; no independent loudness normalization or sample repair
is used.</p>
<p>The 16 kHz and AAC versions are offline FFmpeg transformations. They isolate sample rate and codec choices; they are not new on-device
micd/logger captures. The denoised version is an explicitly experimental FFT filter (6 dB reduction setting, fixed -60 dB noise-floor setting).
No denoising has been installed on the device. Reduced RMS does not establish improved speech quality.</p>
<p>This is the best currently accessible digital capture path, not a calibrated acoustic ceiling. Room sound, mic placement, enclosure, and
firmware decimation still affect it. This recording does not measure the microphone's self-noise, frequency response, or acoustic overload point.
Listen on headphones to assess recording quality independently of the device speaker.</p>
'''
  page = page.replace('DURATION_SECONDS', f'{duration:g}')
  page += '<p>' + html.escape(args.description) + '</p>'
  page += f'<p>Shared playback gain: {gain:.3f}×. Source SHA-256: {metadata["source_sha256"]}.</p>' + ''.join(cards)
  page += (
    '<img alt="Spectral comparison" src="'
    + image
    + '"><table><tr><th>Variant</th><th>RMS dBFS</th><th>Peak dBFS</th><th>Near-rail samples</th></tr>'
    + ''.join(rows)
    + '</table>'
  )
  page += '''<script>document.querySelectorAll('audio').forEach(a=>a.addEventListener('play',()=>{
  document.querySelectorAll('audio').forEach(b=>{if(a!==b)b.pause();});
}));</script>'''
  (args.output / 'index.html').write_text(page)
  (args.output / 'metrics.json').write_text(json.dumps(metadata, indent=2) + '\n')
  print(json.dumps({'report': str(args.output / 'index.html'), 'duration_seconds': duration, 'playback_gain': gain, 'variants': metadata['variants']}))


if __name__ == '__main__':
  main()
