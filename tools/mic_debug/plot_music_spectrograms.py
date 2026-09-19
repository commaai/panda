#!/usr/bin/env python3
"""Align a studio reference and compare capture spectrograms with shared scales."""

import argparse
import base64
import hashlib
import json
import subprocess
from pathlib import Path

import matplotlib
import matplotlib.pyplot as plt
import numpy as np

matplotlib.use('Agg')

SAMPLE_RATE = 48000
MATCH_RATE = 2000
FFT_SIZE = 2048
HOP = 512


def decode(path, rate, channels, filters=None, start=None, duration=None):
  command = ['/usr/bin/ffmpeg', '-v', 'error']
  if start is not None:
    command.extend(['-ss', str(start)])
  command.extend(['-i', str(path)])
  if duration is not None:
    command.extend(['-t', str(duration)])
  if filters:
    command.extend(['-af', filters])
  command.extend(['-ar', str(rate), '-ac', str(channels), '-f', 'f32le', '-'])
  data = subprocess.run(command, check=True, capture_output=True).stdout
  return np.frombuffer(data, dtype='<f4').reshape(-1, channels).astype(float)


def correlation(reference, query):
  size = 1 << (len(reference) + len(query) - 1).bit_length()
  values = np.fft.irfft(np.fft.rfft(reference, size) * np.conj(np.fft.rfft(query, size)), size)[: len(reference) - len(query) + 1]
  cumulative = np.r_[0, np.cumsum(reference**2)]
  energies = cumulative[len(query) :] - cumulative[: -len(query)]
  return values / np.sqrt(np.maximum(energies * np.sum(query**2), 1e-30))


def spectrogram(signal):
  window = np.hanning(FFT_SIZE)
  powers = []
  for channel in signal.T:
    frames = np.lib.stride_tricks.sliding_window_view(channel, FFT_SIZE)[::HOP]
    spectrum = np.fft.rfft(frames * window, axis=1)
    power = np.abs(spectrum) ** 2 / (SAMPLE_RATE * np.sum(window**2))
    power[:, 1:-1] *= 2
    powers.append(power.T)
  return np.mean(powers, axis=0)


def main():
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument('reference', type=Path)
  parser.add_argument('capture', type=Path)
  parser.add_argument('pcm16', type=Path)
  parser.add_argument('aac16', type=Path)
  parser.add_argument('output', type=Path)
  parser.add_argument('--offset', type=float, required=True)
  parser.add_argument('--source-url', required=True)
  args = parser.parse_args()
  args.output.mkdir(parents=True, exist_ok=True)
  captured_match = decode(args.capture, MATCH_RATE, 1, 'highpass=f=120,lowpass=f=800')[:, 0]
  reference_match = decode(args.reference, MATCH_RATE, 1, 'highpass=f=120,lowpass=f=800')[:, 0]
  matches = []
  for start in [5, 13, 21, 29, 37]:
    query = captured_match[start * MATCH_RATE : (start + 6) * MATCH_RATE]
    lower = round((args.offset + start - 0.04) * MATCH_RATE)
    upper = round((args.offset + start + 6 + 0.04) * MATCH_RATE)
    scores = correlation(reference_match[lower:upper], query)
    index = int(np.argmax(scores))
    matches.append({'capture_seconds': start, 'source_seconds': (lower + index) / MATCH_RATE, 'correlation': float(scores[index])})
  capture_positions = np.array([item['capture_seconds'] for item in matches])
  source_positions = np.array([item['source_seconds'] for item in matches])
  slope, offset = np.polyfit(capture_positions, source_positions, 1)
  residual_ms = (source_positions - (offset + slope * capture_positions)) * 1000
  # A small clock-rate correction is fitted only to align matching events in the figure.
  assert abs(slope - 1) < 0.002, slope
  native = decode(args.capture, SAMPLE_RATE, 1)
  length = len(native)
  chunk_start = max(0, offset - 1)
  # Use a high-rate reference for fractional timing alignment so that interpolation
  # does not itself cause appreciable high-frequency loss in the plotted source.
  reference_high_rate = decode(args.reference, SAMPLE_RATE * 4, 2, start=chunk_start, duration=length / SAMPLE_RATE + 2)
  sample_positions = (offset - chunk_start + slope * np.arange(length) / SAMPLE_RATE) * SAMPLE_RATE * 4
  indices = np.floor(sample_positions).astype(int)
  assert indices.min() >= 0 and indices.max() + 1 < len(reference_high_rate)
  fractions = sample_positions - indices
  reference_aligned = reference_high_rate[indices] * (1 - fractions[:, None]) + reference_high_rate[indices + 1] * fractions[:, None]
  pcm16 = decode(args.pcm16, SAMPLE_RATE, 1)
  aac16 = decode(args.aac16, SAMPLE_RATE, 1)
  assert len(pcm16) == length and len(aac16) == length
  tracks = [
    ('Studio radio-edit reference (YouTube Opus, aligned; stereo power average)', reference_aligned),
    ('Comma four native 48 kHz capture (phone speaker + room + microphone + capture path)', native),
    ('Same capture reduced offline to 16 kHz PCM', pcm16),
    ('Same capture reduced offline to 16 kHz AAC at 32 kbit/s', aac16),
  ]
  powers = [spectrogram(signal) for _, signal in tracks]
  frequencies = np.fft.rfftfreq(FFT_SIZE, 1 / SAMPLE_RATE)
  times = (np.arange(powers[0].shape[1]) * HOP + FFT_SIZE / 2) / SAMPLE_RATE
  midband = (frequencies >= 500) & (frequencies <= 2000)
  reference_level = float(np.mean(np.sum(powers[0][midband], axis=0)) * (frequencies[1] - frequencies[0]))
  native_level = float(np.mean(np.sum(powers[1][midband], axis=0)) * (frequencies[1] - frequencies[0]))
  normalized = [powers[0] / reference_level] + [power / native_level for power in powers[1:]]
  figure, axes = plt.subplots(4, 1, figsize=(14, 13), sharex=True, sharey=True, constrained_layout=True)
  for axis, (title, _signal), power in zip(axes, tracks, normalized, strict=True):
    mesh = axis.pcolormesh(times, frequencies / 1000, 10 * np.log10(np.maximum(power, 1e-15)), shading='auto', vmin=-110, vmax=-10, rasterized=True)
    axis.set(title=title, ylabel='Frequency (kHz)', ylim=(0, 24), yticks=[0, 4, 8, 12, 16, 20, 24])
  axes[-1].set_xlabel('Seconds from capture start (reference aligned to the same musical passage)')
  figure.colorbar(mesh, ax=axes, label='dB/Hz relative to matched 500–2000 Hz band power')
  figure.suptitle('Music spectrograms — shared frequency and color scales')
  figure.savefig(args.output / 'spectrograms.png', dpi=150)
  plt.close(figure)
  figure, axis = plt.subplots(figsize=(12, 5), constrained_layout=True)
  for (title, _signal), power in zip(tracks, normalized, strict=True):
    axis.plot(frequencies / 1000, 10 * np.log10(np.maximum(np.mean(power, axis=1), 1e-15)), label=title)
  axis.set(xlabel='Frequency (kHz)', ylabel='Mean normalized power (dB/Hz)', xlim=(0, 24), title='Mean spectra; same normalization as the heatmaps')
  axis.legend(fontsize='small')
  figure.savefig(args.output / 'mean-spectra.png', dpi=150)
  plt.close(figure)
  ratios = {}
  for low, high in [(500, 2000), (2000, 4000), (4000, 8000), (8000, 12000), (12000, 15000), (15000, 16000), (16000, 18000), (18000, 20000)]:
    mask = (frequencies >= low) & (frequencies < high)
    ratios[f'{low}-{high}_Hz'] = float(10 * np.log10(np.sum(normalized[1][mask]) / np.sum(normalized[0][mask])))
  metadata = {
    'source_url': args.source_url,
    'source_codec': 'YouTube Opus format 251, not Spotify PCM or a lossless master',
    'capture_sha256': hashlib.sha256(args.capture.read_bytes()).hexdigest(),
    'source_sha256': hashlib.sha256(args.reference.read_bytes()).hexdigest(),
    'duration_seconds': length / SAMPLE_RATE,
    'capture_start_in_source_seconds': float(offset),
    'source_seconds_per_capture_second': float(slope),
    'window_matches': matches,
    'alignment_residual_ms': residual_ms.tolist(),
    'fft_size': FFT_SIZE,
    'hop_samples': HOP,
    'frequency_resolution_hz': SAMPLE_RATE / FFT_SIZE,
    'frame_spacing_seconds': HOP / SAMPLE_RATE,
    'normalization': 'Reference and native matched by integrated 500–2000 Hz power; native scale reused unchanged for 16 kHz variants.',
    'source_stereo_policy': 'Average channel powers, not coherent mono sum; avoids source stereo cancellation.',
    'native_relative_to_reference_band_db': ratios,
    'scope': 'Combined phone speaker, room, mic/enclosure, MCU and Qualcomm capture path, plus source-stream differences. Not mic-only response.',
  }
  (args.output / 'metrics.json').write_text(json.dumps(metadata, indent=2) + '\n')
  page = '''<!doctype html><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Music frequency versus time</title>
<style>body{font:17px/1.5 system-ui;margin:25px auto;max-width:1400px;padding:0 20px}img{width:100%}pre{white-space:pre-wrap}</style>
<h1>Music frequency versus time</h1>
<p>The studio radio-edit reference is aligned with the recorded musical passage. It is official YouTube audio encoded as Opus,
not the exact Spotify stream or a lossless master.
The reference and native capture are level-matched using 500–2000 Hz band power; the same capture gain applies to both 16 kHz derivatives.
All panels share frequency axes and color limits. Reference stereo channel powers are averaged to avoid introducing a downmix cancellation notch.</p>
<p>The native trace includes the iPhone speaker, room, microphone/enclosure, MCU filtering/resampling, and Qualcomm capture path.
A loss visible there cannot be assigned to the microphone alone. The 16 kHz conversion imposes a separate cutoff below 8 kHz.</p>
'''
  page += (
    '<p><strong>Observed:</strong> Native capture loses substantial treble relative to the reference and reaches its noise floor around 15–16 kHz. '
    + 'The 16 kHz derivatives discard additional content around 8 kHz. '
    + 'These are measurements of this music take, not an isolated microphone frequency-response test.</p>'
  )
  page += f'<p>Native/reference band energy after midrange matching: 4–8 kHz {ratios["4000-8000_Hz"]:.1f} dB; 8–12 kHz {ratios["8000-12000_Hz"]:.1f} dB.</p>'
  page += (
    f'<p>Reference timing at capture start: {offset:.4f} s. Fitted time scale: {slope:.8f}. '
    + f'Maximum local alignment residual: {np.max(np.abs(residual_ms)):.2f} ms.</p>'
  )
  for filename in ['spectrograms.png', 'mean-spectra.png']:
    uri = 'data:image/png;base64,' + base64.b64encode((args.output / filename).read_bytes()).decode()
    page += '<img alt="' + filename + '" src="' + uri + '">'
  page += '<p>No audio download or external asset is required to view this file.</p>'
  (args.output / 'index.html').write_text(page)
  print(json.dumps(metadata))


if __name__ == '__main__':
  main()
