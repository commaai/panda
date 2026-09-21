#!/usr/bin/env python3
"""Audition bounded compensation for the existing DFSDM sinc4 response, offline."""

import argparse
import base64
import hashlib
import json
from pathlib import Path

import matplotlib

matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np

from quality_report import read_wav, wav_bytes

MIC_CLOCK_HZ = 240e6 / 91
DECIMATION = 55
FILTER_ORDER = 4
FIR_TAPS = 257
DESIGN_SIZE = 8192


def decimator_response(frequencies):
  return (np.sinc(frequencies * DECIMATION / MIC_CLOCK_HZ) / np.sinc(frequencies / MIC_CLOCK_HZ)) ** FILTER_ORDER


def main():
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument('source', type=Path)
  parser.add_argument('output', type=Path)
  args = parser.parse_args()
  args.output.mkdir(parents=True, exist_ok=True)
  rate, source = read_wav(args.source)
  assert rate == 48000
  duration = len(source) / rate
  design_frequencies = np.fft.rfftfreq(DESIGN_SIZE, 1 / rate)
  modeled_response = decimator_response(design_frequencies)
  target_boost_db = -20 * np.log10(modeled_response)
  processed = [('native', 'Native 48 kHz PCM', source)]
  figure, axes = plt.subplots(2, 1, figsize=(10, 8), constrained_layout=True)
  axes[0].plot(design_frequencies, 20 * np.log10(modeled_response), label='Calculated existing sinc4 response')
  metadata = {
    'source_sha256': hashlib.sha256(args.source.read_bytes()).hexdigest(),
    'duration_seconds': duration,
    'scope': 'Offline compensation for modeled DFSDM response only; no denoising or measured acoustic EQ.',
    'mic_clock_hz': MIC_CLOCK_HZ,
    'decimation': DECIMATION,
    'filter_order': FILTER_ORDER,
    'fir_taps': FIR_TAPS,
    'variants': [],
  }
  probe_frequencies = np.array([1000, 4000, 8000, 12000, 16000, 20000])
  for cap_db in [3, 6]:
    desired = 10 ** (np.minimum(target_boost_db, cap_db) / 20)
    impulse = np.fft.fftshift(np.fft.irfft(desired, n=DESIGN_SIZE))
    half = FIR_TAPS // 2
    taps = impulse[DESIGN_SIZE // 2 - half : DESIGN_SIZE // 2 + half + 1] * np.hamming(FIR_TAPS)
    taps /= np.sum(taps)
    assert np.allclose(taps, taps[::-1], atol=1e-12)
    filtered = np.convolve(source, taps, mode='same')
    assert len(filtered) == len(source) and np.all(np.isfinite(filtered))
    key = f'compensated{cap_db}'
    processed.append((key, f'Filter compensation capped at +{cap_db} dB', filtered))
    response = np.abs(np.fft.rfft(taps, n=65536))
    frequencies = np.fft.rfftfreq(65536, 1 / rate)
    expected = np.interp(frequencies, design_frequencies, np.minimum(target_boost_db, cap_db))
    actual_db = 20 * np.log10(response)
    error = float(np.max(np.abs(actual_db - expected)))
    assert error < 0.1, error
    assert np.max(actual_db) <= cap_db + 0.02
    axes[0].plot(frequencies, 20 * np.log10(decimator_response(frequencies) * response), label=f'Sinc4 plus +{cap_db} dB capped compensation')
    axes[1].plot(frequencies, actual_db, label=f'Applied +{cap_db} dB capped filter')
    np.save(args.output / (key + '_taps.npy'), taps)
    metrics = {
      'name': key,
      'cap_db': cap_db,
      'maximum_response_error_db': error,
      'correction_db': dict(zip(probe_frequencies.astype(str).tolist(), np.interp(probe_frequencies, frequencies, actual_db).tolist(), strict=True)),
      'rms_change_db': float(20 * np.log10(np.std(filtered) / np.std(source))),
      'peak_before_playback_gain': float(np.max(np.abs(filtered))),
    }
    metadata['variants'].append(metrics)
  for axis in axes:
    axis.set(xlabel='Frequency (Hz)', ylabel='Gain (dB)', xlim=(0, 20000))
    axis.legend()
  axes[0].set_title('Calculated filter response, not measured microphone response')
  axes[1].set_title('Offline correction applied to this listening comparison')
  figure.savefig(args.output / 'filter-response.png')
  plt.close(figure)
  gain = min(128, 0.7 / max(np.max(np.abs(signal)) for _, _, signal in processed))
  metadata['shared_playback_gain'] = gain
  cards = []
  for key, title, signal in processed:
    audio = wav_bytes(signal * gain, rate)
    (args.output / (key + '.wav')).write_bytes(audio)
    uri = 'data:audio/wav;base64,' + base64.b64encode(audio).decode()
    cards.append(f'<section><h2>{title}</h2><audio controls preload="none" src="{uri}"></audio></section>')
  image = 'data:image/png;base64,' + base64.b64encode((args.output / 'filter-response.png').read_bytes()).decode()
  page = '''<!doctype html><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Native microphone treble comparison</title><style>
body{font:17px/1.5 system-ui;max-width:1000px;margin:30px auto;padding:0 20px}audio,img{width:100%}
section{border:1px solid #ccc;padding:16px;margin:16px 0}h2{font-size:20px}
</style><h1>Treble experiment: same native recording</h1>
<p>All three players use the same iPhone music recording at 48 kHz PCM, with one shared playback gain.
The two experimental versions apply a fixed filter that partially compensates the known sinc4 decimator's calculated treble loss.
Boost is capped at 3 or 6 dB. There is no denoising, dynamic processing, or acoustic EQ fitted to the song.</p>
<p>This is an offline listening experiment, not an installed firmware change or proof of improved fidelity.
The correction raises high-frequency noise along with music. It does not compensate the phone speaker, room, enclosure,
microphone response or cubic resampler. The symmetric 257-tap FIR is centered to remove its delay for listening alignment;
real-time use would require accounting for its 128-sample delay.</p>
<p>Listen on headphones. Start with native versus the 3 dB cap; use the 6 dB cap to hear a stronger correction.
A preference for more treble alone does not identify why the original sounds muffled.</p>
'''
  page += f'<p>Duration: {duration:g} seconds. Shared gain: {gain:.4f}×.</p>' + ''.join(cards)
  page += '<img alt="Modeled decimator and experimental correction responses" src="' + image + '">'
  page += '''<script>document.querySelectorAll('audio').forEach(a=>a.addEventListener('play',()=>{
    document.querySelectorAll('audio').forEach(b=>{if(a!==b)b.pause();});
  }));</script>'''
  (args.output / 'index.html').write_text(page)
  (args.output / 'metrics.json').write_text(json.dumps(metadata, indent=2) + '\n')
  print(json.dumps(metadata))


if __name__ == '__main__':
  main()
