import base64
import json
import wave
from pathlib import Path

import matplotlib
import numpy as np

matplotlib.use('Agg')
import matplotlib.pyplot as plt

ROOT = Path('/home/batman/tmp/c4-mic-results/headphone-source-01')


def band_db(samples, rate, center):
  window = np.hanning(len(samples))
  frequencies = np.fft.rfftfreq(len(samples), 1 / rate)
  power = abs(np.fft.rfft(samples * window)) ** 2
  energy = 2 * power[abs(frequencies - center) < 25].sum() / (len(samples) * np.sum(window ** 2))
  return float(10 * np.log10(max(energy, 1e-30)))


def main():
  raw = np.load(ROOT / 'captured.npz')
  samples = raw['samples'][:, 0].astype(float) / 32768
  metadata = json.loads((ROOT / 'captured.json').read_text())
  rate = int(metadata['rate'])
  size, hop = 4096, 480
  window = np.hanning(size)
  blocks = np.lib.stride_tricks.sliding_window_view(samples, size)[::hop]
  power = abs(np.fft.rfft(blocks * window, axis=1)) ** 2 / (rate * np.sum(window ** 2))
  power[:, 1:-1] *= 2
  frequencies = np.fft.rfftfreq(size, 1 / rate)
  times = (np.arange(len(power)) * hop + size / 2) / rate
  energy = power[:, abs(frequencies - 1000) < 30].sum(axis=1)
  width = 180
  scores = np.convolve(energy, np.ones(width), mode='valid')
  centers = times[:len(scores)] + (width - 1) * hop / rate / 2
  search = np.flatnonzero((centers > 2) & (centers < 8))
  offset = float(centers[search[np.argmax(scores[search])]] - 3)
  schedule = json.loads((ROOT / 'stimulus.json').read_text())['schedule']
  rows = []
  for item in schedule:
    start = item['start_seconds'] + offset
    active = samples[round((start + .25) * rate):round((start + 1.75) * rate)]
    quiet = samples[round((start - .75) * rate):round((start - .15) * rate)]
    frequency = item['frequency_hz']
    tone = band_db(active, rate, frequency)
    noise = band_db(quiet, rate, frequency)
    bins = np.fft.rfftfreq(len(active), 1 / rate)
    active_power = abs(np.fft.rfft(active * np.hanning(len(active)))) ** 2
    candidates = np.flatnonzero(abs(bins - frequency) < 25)
    peak_frequency = float(bins[candidates[np.argmax(active_power[candidates])]])
    rows.append(dict(item, tone_band_dbfs=tone, quiet_band_dbfs=noise, rise_db=tone-noise, peak_frequency_hz=peak_frequency))
  verified = rows[0]['rise_db'] > 15 and rows[1]['rise_db'] > 15
  metrics = {'dataset': 'headphone-source-01', 'source': 'USB AirPods Max, open earcup near USB corner',
             'source_offset_seconds': offset, 'low_frequency_playback_verified': verified,
             'capture_seconds': len(samples) / rate, 'peak_dbfs': float(20 * np.log10(max(abs(samples).max(), 1e-20))),
             'rail_samples': int(np.count_nonzero(abs(raw['samples'].astype(np.int32)) >= 32767)),
             'input_overflows': int(raw['callbacks'][:, 5].sum()),
             'input_underflows': int(raw['callbacks'][:, 6].sum()),
             'playback_underflows': metadata['playback_underflows'],
             'firmware': metadata['panda_version'], 'tones': rows,
             'limitations': 'Uncalibrated source and placement. Measured response includes headphone, air path, enclosure and microphone processing. Positive detection is useful; non-detection cannot identify which element attenuates the tone. No EQ, denoising or simulated microphone distortion.'}
  (ROOT / 'metrics.json').write_text(json.dumps(metrics, indent=2) + '\n')
  figure, axes = plt.subplots(2, 1, figsize=(12, 9), constrained_layout=True)
  heatmap = axes[0].pcolormesh(times, frequencies / 1000, 10 * np.log10(np.maximum(power.T, 1e-20)), shading='auto', vmin=-145, vmax=-65)
  figure.colorbar(heatmap, ax=axes[0], label='Power spectral density (dBFS/Hz)')
  axes[0].set(xlabel='Capture time (s)', ylabel='Frequency (kHz)', ylim=(0, 24), title='Actual AirPods Max → comma four microphone recording')
  for key, label in [('tone_band_dbfs', 'During scheduled tone'), ('quiet_band_dbfs', 'Preceding quiet interval')]:
    axes[1].plot([row['frequency_hz'] / 1000 for row in rows], [row[key] for row in rows], marker='o', label=label)
  axes[1].set(xlabel='Frequency (kHz)', ylabel='RMS in ±25 Hz band (dBFS)', title='Combined source + placement + microphone; not a calibrated mic response')
  axes[1].legend()
  figure.savefig(ROOT / 'headphone-tones.png', dpi=150)
  page = '<!doctype html><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Independent headphone microphone test</title><style>body{font:17px/1.5 system-ui;max-width:1200px;margin:25px auto;padding:0 20px}img,audio{width:100%}td,th{padding:6px;text-align:right}</style><h1>Independent headphone source test</h1><p>Real 45-second native 48 kHz capture. USB AirPods Max played 0.1-peak tones from 1 to 22 kHz near the USB corner of the flat comma four. Spoken countdown preceded recording; the comma four output digital silence during capture to maintain its audio clocks. No EQ or denoising was applied.</p>'
  page += '<p><strong>Tones were detected through 22 kHz.</strong> At 20, 21 and 22 kHz, the scheduled-tone band rose about 13, 31 and 22 dB above its preceding quiet interval. This rules out a hard 15–18 kHz cutoff in this capture path. The earlier iPhone non-detection cannot be attributed to a hard microphone cutoff; source and placement matter. The 20 kHz dip remains a combined source/placement/microphone observation.</p>'
  page += '<p>' + metrics['limitations'] + '</p>'
  page += f'<p>Low-frequency playback verified in capture: {verified}. Input overruns: {metrics["input_overflows"]}. Samples at integer rails: {metrics["rail_samples"]}.</p>'
  page += '<img alt="Actual headphone tone capture and narrowband levels" src="data:image/png;base64,' + base64.b64encode((ROOT / 'headphone-tones.png').read_bytes()).decode() + '">'
  page += '<table><tr><th>Frequency (kHz)</th><th>Tone (dBFS)</th><th>Quiet (dBFS)</th><th>Rise (dB)</th></tr>'
  for row in rows:
    page += f'<tr><td>{row["frequency_hz"]/1000:g}</td><td>{row["tone_band_dbfs"]:.1f}</td><td>{row["quiet_band_dbfs"]:.1f}</td><td>{row["rise_db"]:.1f}</td></tr>'
  page += '</table>'
  for name, title in [('headphone-tones.wav', 'Source file played through headphones'), ('captured.wav', 'Actual microphone recording, original level')]:
    page += '<h2>' + title + '</h2><audio controls preload="none" src="data:audio/wav;base64,' + base64.b64encode((ROOT / name).read_bytes()).decode() + '"></audio>'
  page += '<p>All audio and figures are embedded. No external assets.</p>'
  (ROOT / 'index.html').write_text(page)
  print(json.dumps(metrics, indent=2))


if __name__ == '__main__':
  main()
