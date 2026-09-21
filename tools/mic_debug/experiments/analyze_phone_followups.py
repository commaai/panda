import base64
import json
import wave
from pathlib import Path

import matplotlib
import numpy as np

matplotlib.use('Agg')
import matplotlib.pyplot as plt

ROOT = Path('/home/batman/tmp/c4-mic-results')
OUTPUT = ROOT / 'phone-followups-report'
OUTPUT.mkdir(exist_ok=True)


def read_wav(path):
  with wave.open(str(path)) as recording:
    assert recording.getsampwidth() == 2
    rate = recording.getframerate()
    samples = np.frombuffer(recording.readframes(recording.getnframes()), dtype='<i2').reshape(-1, recording.getnchannels()) / 32768.
  return rate, samples


def spectrum(samples, rate):
  if samples.ndim == 1:
    samples = samples[:, None]
  size, hop = 4096, 480
  window = np.hanning(size)
  channel_power = []
  for channel in samples.T:
    frames = np.lib.stride_tricks.sliding_window_view(channel, size)[::hop]
    power = abs(np.fft.rfft(frames * window, axis=1)) ** 2 / (rate * np.sum(window ** 2))
    power[:, 1:-1] *= 2
    channel_power.append(power)
  times = (np.arange(len(power)) * hop + size / 2) / rate
  frequency = np.fft.rfftfreq(size, 1 / rate)
  return times, frequency, np.mean(channel_power, axis=0)


def band_db(samples, rate, frequency):
  window = np.hanning(len(samples))
  bins = np.fft.rfftfreq(len(samples), 1 / rate)
  power = abs(np.fft.rfft(samples * window)) ** 2
  rms_squared = 2 * power[abs(bins - frequency) < 25].sum() / (len(samples) * np.sum(window ** 2))
  return float(10 * np.log10(max(rms_squared, 1e-30)))


rate, levels = read_wav(ROOT / 'phone-levels-01/captured.wav')
levels = levels[:, 0]
times, frequencies, power = spectrum(levels, rate)
energy = power[:, abs(frequencies - 1000) < 30].sum(axis=1)
width = 80
scores = np.convolve(energy, np.ones(width), mode='valid')
centers = times[:len(scores)] + (width - 1) * .005
selected = (centers > 2) & (centers < 5)
center = centers[np.flatnonzero(selected)[np.argmax(scores[selected])]]
offset = float(center - 2.5)
schedule = json.loads((ROOT / 'phone-levels-01/stimulus.json').read_text())['schedule']
measured = []
for item in schedule:
  start = item['start'] + offset
  tone = levels[round((start + .2) * rate):round((start + .8) * rate)]
  quiet = levels[round((start - .35) * rate):round((start - .05) * rate)]
  active = band_db(tone, rate, item['frequency_hz'])
  noise = band_db(quiet, rate, item['frequency_hz'])
  measured.append(dict(item, tone_band_dbfs=active, quiet_band_dbfs=noise, rise_db=active - noise))
assert measured[0]['rise_db'] > 15, 'Phone tone not verified'
figure, axes = plt.subplots(2, 1, figsize=(12, 9), constrained_layout=True)
heatmap = axes[0].pcolormesh(times, frequencies / 1000, 10 * np.log10(np.maximum(power.T, 1e-20)), shading='auto', vmin=-145, vmax=-65)
figure.colorbar(heatmap, ax=axes[0], label='Power spectral density (dBFS/Hz)')
axes[0].set(xlabel='Capture time (s)', ylabel='Frequency (kHz)', ylim=(0, 24), title='iPhone level test: actual mic recording; three increasing source levels')
for peak in [.025, .1, .2]:
  group = [item for item in measured if item['peak'] == peak]
  axes[1].plot([item['frequency_hz'] / 1000 for item in group], [item['tone_band_dbfs'] for item in group], marker='o', label=f'Source peak {peak:g}')
axes[1].set(xlabel='Frequency (kHz)', ylabel='RMS in ±25 Hz band (dBFS)', title='Phone + placement + mic combined; includes noise when tone is undetected')
axes[1].legend()
figure.savefig(OUTPUT / 'levels.png', dpi=150)
plt.close(figure)

rate, source = read_wav(ROOT / 'phone-music-01/phone-music.wav')
_, recording = read_wav(ROOT / 'phone-music-01/captured.wav')
source_times, frequencies, source_power = spectrum(source, rate)
capture_times, _, capture_power = spectrum(recording, rate)
midband = (frequencies >= 500) & (frequencies < 2000)
source_envelope = np.log(np.maximum(source_power[:, midband].sum(axis=1), 1e-20))
capture_envelope = np.log(np.maximum(capture_power[:, midband].sum(axis=1), 1e-20))
region = (source_times > 3) & (source_times < 46)
reference = source_envelope[region]
reference -= np.mean(reference)
offsets = np.arange(0, 3, .01)
correlations = []
for delay in offsets:
  candidate = np.interp(source_times[region] + delay, capture_times, capture_envelope)
  candidate -= np.mean(candidate)
  correlations.append(float(np.dot(reference, candidate) / np.sqrt(np.dot(reference, reference) * np.dot(candidate, candidate))))
music_offset = float(offsets[np.argmax(correlations)])
assert max(correlations) > .4, 'Music temporal match is weak; inspect before interpreting'
music_region = (capture_times > music_offset + 3) & (capture_times < music_offset + 46)
quiet_region = capture_times > music_offset + 49
bands = []
for lower, upper in [(500, 2000), (4000, 8000), (8000, 12000), (12000, 14000), (14000, 16000), (16000, 18000), (18000, 20000)]:
  band = (frequencies >= lower) & (frequencies < upper)
  signal = float(10 * np.log10(capture_power[music_region][:, band].mean(axis=0).sum() * (frequencies[1] - frequencies[0])))
  noise = float(10 * np.log10(capture_power[quiet_region][:, band].mean(axis=0).sum() * (frequencies[1] - frequencies[0])))
  bands.append({'band_hz': [lower, upper], 'music_dbfs': signal, 'post_music_noise_dbfs': noise, 'rise_db': signal-noise})
figure, axes = plt.subplots(2, 1, figsize=(12, 8), constrained_layout=True)
for axis, plot_times, plot_power, mask, title in [(axes[0], source_times - 2, source_power, region, 'File played on iPhone: radio-edit reference, stereo power average'), (axes[1], capture_times - music_offset - 2, capture_power, music_region, 'Actual iPhone → c4 microphone capture; same source segment')]:
  midpoint = 10 * np.log10(plot_power[mask][:, midband].mean())
  heatmap = axis.pcolormesh(plot_times, frequencies / 1000, 10 * np.log10(np.maximum(plot_power.T, 1e-20)) - midpoint, shading='auto', vmin=-85, vmax=15)
  axis.set(xlabel='Music time (s)', ylabel='Frequency (kHz)', ylim=(0, 24), xlim=(0, 45), title=title)
figure.colorbar(heatmap, ax=axes, label='PSD relative to each signal’s mean 500–2000 Hz PSD (dB)')
figure.savefig(OUTPUT / 'music.png', dpi=150)
plt.close(figure)
metrics = {'levels': measured, 'level_source_offset_seconds': offset, 'music_source_offset_seconds': music_offset, 'music_envelope_correlation': max(correlations), 'music_bands': bands, 'scope': 'Real acoustic captures. Source file levels deliberately changed; physical phone volume and capture firmware unchanged. No correction EQ or denoising.'}
for name in ['phone-levels-01', 'phone-music-01']:
  raw = np.load(ROOT / name / 'captured.npz')
  metrics[name + '_overflows'] = int(raw['callbacks'][:, 5].sum())
(OUTPUT / 'metrics.json').write_text(json.dumps(metrics, indent=2) + '\n')
page = '''<!doctype html><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Comma four: phone music and level tests</title><style>body{font:17px/1.5 system-ui;max-width:1200px;margin:25px auto;padding:0 20px}img,audio{width:100%}</style><h1>Same phone: music and tones</h1><p>These are actual physical microphone recordings. The phone played a 45-second excerpt of the previously downloaded radio-edit reference, then a tone sequence at three digital levels. The phone's physical volume setting was kept unchanged, approximately 60%. The music file was scaled to 0.2 full-scale peak; tone peaks were 0.025, 0.1 and 0.2. Those are controlled digital file levels, not measured sound-pressure levels. Both recordings use the same pop-fix firmware and native 48 kHz PCM capture. No EQ or denoising was applied. This still measures the source speaker, placement, enclosure and mic together.</p><p>The earlier built-in-speaker result through 22 kHz used a different source and coupling; it is not interchangeable with this external-phone test. The original Spotify recording also used a different playback app and source stream.</p>'''
for name in ['music.png', 'levels.png']:
  page += '<img alt="' + name + '" src="data:image/png;base64,' + base64.b64encode((OUTPUT / name).read_bytes()).decode() + '">'
for label, path in [('Music file played on phone (source, not mic)', ROOT / 'phone-music-01/phone-music.wav'), ('Actual new music recording (original level)', ROOT / 'phone-music-01/captured.wav'), ('Actual level-test recording (original level)', ROOT / 'phone-levels-01/captured.wav')]:
  page += '<h2>' + label + '</h2><audio controls preload="none" src="data:audio/wav;base64,' + base64.b64encode(path.read_bytes()).decode() + '"></audio>'
page += '<p>All images and audio are embedded; no external assets are loaded. Playback levels are not perceptually loudness-matched.</p>'
(OUTPUT / 'index.html').write_text(page)
print(json.dumps(metrics))
